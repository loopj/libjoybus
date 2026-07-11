/**
 * Espressif ESP32 implementation
 */

#include <joybus/bus.h>
#include <joybus/errors.h>
#include <joybus/target.h>
#include <joybus/backend/esp32.h>

#include <esp_attr.h>
#include <esp_clk_tree.h>
#include <esp_cpu.h>
#include <esp_idf_version.h>
#include <esp_intr_alloc.h>
#include <esp_rom_gpio.h>
#include <esp_rom_sys.h>
#include <esp_timer.h>
#include <esp_private/periph_ctrl.h>
#include <hal/rmt_ll.h>
#include <soc/clk_tree_defs.h>
#include <soc/soc_caps.h>

// Compatibility macros for ESP-IDF v5 vs v6
#if ESP_IDF_VERSION_MAJOR >= 6
#include <hal/rmt_periph.h>
#define JOYBUS_RMT_GROUP0             soc_rmt_signals[0]
#define JOYBUS_RMT_CHANNELS_PER_GROUP RMT_LL_CHANS_PER_INST
#define JOYBUS_RMT_TX_CANDIDATES      RMT_LL_TX_CANDIDATES_PER_INST
#else
#include <soc/rmt_periph.h>
#define JOYBUS_RMT_GROUP0             rmt_periph_signals.groups[0]
#define JOYBUS_RMT_CHANNELS_PER_GROUP SOC_RMT_CHANNELS_PER_GROUP
#define JOYBUS_RMT_TX_CANDIDATES      SOC_RMT_TX_CANDIDATES_PER_GROUP
#endif

// Internal bus state
enum {
  BUS_STATE_DISABLED,
  BUS_STATE_HOST_IDLE,
  BUS_STATE_HOST_TX_WAIT,
  BUS_STATE_HOST_TX,
  BUS_STATE_HOST_RX,
  BUS_STATE_TARGET_RX,
  BUS_STATE_TARGET_TX,
  BUS_STATE_TARGET_ERROR,
};

// 8 bits = 8 RMT symbols = 1 byte
#define SYMBOLS_PER_BYTE  8

// RX capture ring buffer size
#define RX_RING SOC_RMT_MEM_WORDS_PER_CHANNEL

// Use lower latency interrupt dispatch method if available
#if CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD
#define TIMER_DISPATCH ESP_TIMER_ISR
#else
#define TIMER_DISPATCH ESP_TIMER_TASK
#endif

// Fire the "byte received" interrupt this many symbols early, so we decode the
// captured bits while the last ones arrive instead of after the byte completes
#if defined(CONFIG_IDF_TARGET_ESP32H2)
#define RX_DECODE_HIDE 2
#else
#define RX_DECODE_HIDE 1
#endif
#if RX_DECODE_HIDE < 0 || RX_DECODE_HIDE >= SYMBOLS_PER_BYTE
#error "RX_DECODE_HIDE must be between 0 and SYMBOLS_PER_BYTE-1"
#endif

// Ensure we never start a target reply (much) sooner than an OEM controller
#define TARGET_REPLY_FLOOR_NS 1500

// 2 blocks, the wrap boundary for the TX refill
#define TX_MEM_SYMS (2 * SOC_RMT_MEM_WORDS_PER_CHANNEL)
#if TX_MEM_SYMS % SYMBOLS_PER_BYTE != 0
#error "TX_MEM_SYMS must be a multiple of SYMBOLS_PER_BYTE"
#endif

// Direct access to RMT memory (linker-provided symbol)
extern volatile rmt_symbol_word_t RMTMEM[JOYBUS_RMT_CHANNELS_PER_GROUP][SOC_RMT_MEM_WORDS_PER_CHANNEL];

// Encode a byte from write_buf into RMT TX memory
static inline IRAM_ATTR void encode_byte(struct joybus_esp32_data *data, uint16_t byte_idx)
{
  volatile rmt_symbol_word_t *mem = RMTMEM[data->rmt_tx_ch];

  // Find our position in the ring buffer
  uint16_t offset = (uint16_t)(byte_idx * SYMBOLS_PER_BYTE % TX_MEM_SYMS);

  // Grab the byte value we are encoding
  uint8_t value = data->write_buf[byte_idx];

  // Encode each bit
  for (uint8_t mask = 0x80; mask != 0; mask >>= 1)
    mem[offset++].val = (value & mask) ? data->sym_one : data->sym_zero;
}

// Encode a stop symbol into RMT TX memory after the last byte
static inline IRAM_ATTR void encode_stop(struct joybus_esp32_data *data)
{
  // Find our position in the ring buffer
  uint16_t offset = (uint16_t)(data->write_len * SYMBOLS_PER_BYTE % TX_MEM_SYMS);

  // Encode the stop symbol
  RMTMEM[data->rmt_tx_ch][offset].val = data->sym_stop;
}

/*
 * Prepare and start a RMT TX.
 *
 * We use a little trick here to decrease the RX->TX turnaround time. We
 * populate only the first symbol/bit, and then start the transfer immediately.
 * The remaining symbols of the first byte and the second byte/stop symbol are
 * then populated while the transfer is ongoing.
 *
 * Subsequent bytes (if any) are then populated as part of the TX_THRESH
 * interrupt handler so we don't ever fill up the RMT TX buffer.
 */
static IRAM_ATTR void start_write(struct joybus *bus)
{
  struct joybus_esp32_data *data  = &JOYBUS_ESP32(bus)->data;
  volatile rmt_symbol_word_t *mem = RMTMEM[data->rmt_tx_ch];

  // Write the first byte's first symbol, fire immediately, then fill the rest of the byte
  uint8_t v0 = data->write_buf[0];
  for (uint16_t off = 0; off < SYMBOLS_PER_BYTE; off++) {
    mem[off].val = (v0 & (0x80 >> off)) ? data->sym_one : data->sym_zero;
    if (off == 0)
      rmt_ll_tx_start(&RMT, data->rmt_tx_ch);
  }
  data->write_count = 1;

  // Encode the next byte (or stop symbol) and enable TX_THRES interrupt to stream the rest
  if (data->write_len > 1) {
    encode_byte(data, data->write_count);
    data->write_count++;

    // Enable TX_THRES interrupt to stream the rest of the bytes
    rmt_ll_clear_interrupt_status(&RMT, RMT_LL_EVENT_TX_THRES(data->rmt_tx_ch));
    rmt_ll_enable_interrupt(&RMT, RMT_LL_EVENT_TX_THRES(data->rmt_tx_ch), true);
  } else {
    // Otherwise, just encode the stop symbol
    encode_stop(data);
  }
}

// Target's send_response callback: stage the reply. Its buffer is long-lived, so we stream straight from it
static void IRAM_ATTR handle_command_response(const uint8_t *response, uint8_t len, void *user_data)
{
  struct joybus_esp32_data *data = &JOYBUS_ESP32((struct joybus *)user_data)->data;

  // Just save the buffer pointer and length for later
  data->write_buf = response;
  data->write_len = len;
}

// Frequency-independent bit decode: '1' is short low + long high, '0' the reverse
static inline uint8_t decode_bit(rmt_symbol_word_t sym)
{
  return sym.duration0 < sym.duration1;
}

// How many symbols the writer has committed at or past `base`, wrapped into [0, RX_RING).
static inline IRAM_ATTR int rx_committed(struct joybus_esp32_data *data, int base)
{
  int dist = (int)rmt_ll_rx_get_memory_writer_offset(&RMT, data->rmt_rx_ch) - base;
  if (dist < 0)
    dist += RX_RING;

  return dist;
}

/*
 * Decode one byte from RMT RX memory, starting at symbol index `base_sym`
 *
 * The "byte received" interrupt can fire before the byte's final symbol is
 * committed (the byte clock is set to land one symbol early), so we fold in
 * each bit the moment its symbol commits and busy-wait only for that final
 * symbol.
 */
static inline IRAM_ATTR uint8_t decode_byte(struct joybus_esp32_data *data, int base_sym)
{
  volatile rmt_symbol_word_t *mem = RMTMEM[data->rmt_rx_mem_ch];

  // Wrap the byte's start into the capture ring
  int base = base_sym % RX_RING;

  // Fill the byte, folding in each bit as its symbol commits. Read the writer offset once and
  // re-read it only when we catch up to it: it barely moves while we read the bits that already
  // landed, and on a slow peripheral bus (the ESP32-H2) re-reading it every bit dominates the decode.
  uint8_t byte      = 0;
  int     committed = rx_committed(data, base);
  for (int i = 0; i < SYMBOLS_PER_BYTE; i++) {
    while (committed <= i)
      committed = rx_committed(data, base);
    byte = (uint8_t)((byte << 1) | decode_bit(mem[(base + i) % RX_RING]));
  }

  return byte;
}

// Enter target RX mode: reset the read state and start capturing the next command.
static IRAM_ATTR void enter_target_rx_mode(struct joybus *bus)
{
  struct joybus_esp32_data *data = &JOYBUS_ESP32(bus)->data;

  data->read_buf   = bus->command_buffer;
  data->read_len   = JOYBUS_BLOCK_SIZE;
  data->read_count = 0;
  data->write_buf  = NULL;
  data->write_len  = 0;

  // Configure RMT RX for a fresh frame, firing the byte-received interrupt RX_DECODE_HIDE
  // symbols early so the decode of the already-captured bits overlaps the last bits arriving
  rmt_ll_rx_set_limit(&RMT, data->rmt_rx_ch, SYMBOLS_PER_BYTE - RX_DECODE_HIDE);
  rmt_ll_rx_set_mem_owner(&RMT, data->rmt_rx_ch, RMT_LL_MEM_OWNER_HW);
  rmt_ll_rx_reset_pointer(&RMT, data->rmt_rx_ch);

  // Reset the TX read pointer now, while we are idle
  rmt_ll_tx_reset_pointer(&RMT, data->rmt_tx_ch);

  // Enable RX
  rmt_ll_rx_enable(&RMT, data->rmt_rx_ch, true);

  // Transition state
  data->state = BUS_STATE_TARGET_RX;
}

// Kick off a transfer initiated by joybus_transfer
static IRAM_ATTR void transfer_start(struct joybus *bus)
{
  struct joybus_esp32_data *data = &JOYBUS_ESP32(bus)->data;

  // Disable RX and clear interrupt status
  rmt_ll_rx_enable(&RMT, data->rmt_rx_ch, false);
  rmt_ll_clear_interrupt_status(&RMT, RMT_LL_EVENT_RX_THRES(data->rmt_rx_ch) | RMT_LL_EVENT_RX_DONE(data->rmt_rx_ch) |
                                        RMT_LL_EVENT_TX_DONE(data->rmt_tx_ch));

  // Prepare to receive a response
  if (data->read_len > 0) {
    // Reset the receive state
    data->read_count = 0;

    // Skip the command bytes in the RX capture, so we only decode the reply
    data->rx_skip = data->write_len;

    // Arm RX before transmitting, so the capture is already running when the
    // reply arrives. RX records our own command, the stop bit, and the reply
    // as one continuous capture, and the reply is decoded afterward from a
    // fixed symbol offset. This is the most reliable way to begin capturing
    // responses to commands, no matter the MCU clock speed.
    rmt_ll_rx_set_limit(&RMT, data->rmt_rx_ch, SYMBOLS_PER_BYTE);
    rmt_ll_rx_set_mem_owner(&RMT, data->rmt_rx_ch, RMT_LL_MEM_OWNER_HW);
    rmt_ll_rx_reset_pointer(&RMT, data->rmt_rx_ch);
    rmt_ll_rx_enable(&RMT, data->rmt_rx_ch, true);
  }

  // Reset the TX read pointer
  rmt_ll_tx_reset_pointer(&RMT, data->rmt_tx_ch);

  // Start the write
  start_write(bus);

  // Transition state
  data->state = BUS_STATE_HOST_TX;
}

// Finish a host transfer and return to host idle state
static IRAM_ATTR void transfer_finish(struct joybus *bus, int status)
{
  struct joybus_esp32_data *data = &JOYBUS_ESP32(bus)->data;

  // A transfer finishes once. Bail if it already returned to idle so the callback fires only once.
  if (data->state == BUS_STATE_HOST_IDLE)
    return;

  // Return to idle BEFORE the callback, since apps could kick off a new transfer from inside done_callback
  data->state = BUS_STATE_HOST_IDLE;

  // Disable RX
  rmt_ll_rx_enable(&RMT, data->rmt_rx_ch, false);

  // Record the completion time for enforcing minimum interval between transfers
  data->last_transfer_us = esp_timer_get_time();

  // Call the transfer complete callback with status
  if (data->done_callback)
    data->done_callback(bus, status, data->done_user_data);
}

// Handle host tx complete (all command bytes sent)
static inline IRAM_ATTR void host_tx_complete(struct joybus *bus)
{
  struct joybus_esp32_data *data = &JOYBUS_ESP32(bus)->data;

  // We've finished sending a command, check if we need to receive a response
  if (data->read_len > 0) {
    // RX is already capturing, so just switch to the receive state
    data->state = BUS_STATE_HOST_RX;
  } else {
    // No reply expected, go idle and call the transfer complete callback
    transfer_finish(bus, 0);
  }
}

// Handle target tx complete (all response bytes sent)
static inline IRAM_ATTR void target_tx_complete(struct joybus *bus)
{
  // Switch back to target RX mode
  enter_target_rx_mode(bus);
}

// Handle a host RX "byte received" interrupt: skip our own captured command, then stream the reply.
static inline IRAM_ATTR void host_byte_received(struct joybus *bus)
{
  struct joybus_esp32_data *data = &JOYBUS_ESP32(bus)->data;

  // RX captures our command too, so the first write_len byte-clock ticks belong to the command
  if (data->rx_skip > 0) {
    data->rx_skip--;
    return;
  }

  // Decode the next reply byte. The reply starts one symbol past the command and its stop bit, so
  // every reply byte lands one symbol early, the same interrupt timing decode_byte handles
  int base_sym = (data->write_len * SYMBOLS_PER_BYTE + 1) + data->read_count * SYMBOLS_PER_BYTE;
  data->read_buf[data->read_count] = decode_byte(data, base_sym);
  data->read_count++;

  // Whole reply captured
  if (data->read_count == data->read_len)
    transfer_finish(bus, 0);
}

// Handle target byte received
static inline IRAM_ATTR void target_byte_received(struct joybus *bus)
{
  struct joybus_esp32_data *data = &JOYBUS_ESP32(bus)->data;

  // Save the received byte in the buffer
  data->read_buf[data->read_count] = decode_byte(data, data->read_count * SYMBOLS_PER_BYTE);
  uint32_t cmd_end = esp_cpu_get_cycle_count();
  data->read_count++;

  // Call the target handler to prepare a response if needed
  int rc = joybus_target_byte_received(bus->target, data->read_buf, data->read_count, handle_command_response, bus);
  if (rc == 0) {
    // No more bytes expected
    if (data->write_len > 0) {
      // A response is expected - switch to target TX mode and kick off the write
      data->state = BUS_STATE_TARGET_TX;

      // Ensure we never reply (much) faster than an OEM controller
      while ((int32_t)(esp_cpu_get_cycle_count() - (cmd_end + data->reply_floor_cycles)) < 0)
        ;

      start_write(bus);

      // Stop capturing and clear interrupt status
      rmt_ll_rx_enable(&RMT, data->rmt_rx_ch, false);
      rmt_ll_clear_interrupt_status(&RMT, RMT_LL_EVENT_RX_DONE(data->rmt_rx_ch));
      rmt_ll_clear_interrupt_status(&RMT, RMT_LL_EVENT_TX_DONE(data->rmt_tx_ch));
    } else {
      // No response to send
      // Stop capturing and clear interrupt status
      rmt_ll_rx_enable(&RMT, data->rmt_rx_ch, false);
      rmt_ll_clear_interrupt_status(&RMT, RMT_LL_EVENT_RX_DONE(data->rmt_rx_ch));

      // Switch back to target read mode
      enter_target_rx_mode(bus);
    }
  } else if (rc > 0) {
    // More bytes expected
    // After the first byte, switch to 8-symbol captures
    if (data->read_count == 1) {
      rmt_ll_rx_set_limit(&RMT, data->rmt_rx_ch, SYMBOLS_PER_BYTE);
      rmt_ll_rx_enable(&RMT, data->rmt_rx_ch, true);
      rmt_ll_clear_interrupt_status(&RMT, RMT_LL_EVENT_RX_THRES(data->rmt_rx_ch));
    }
  } else {
    // Error handling command, or command not supported
    // RX_DONE will re-enable RX after the bus becomes idle
    data->state = BUS_STATE_TARGET_ERROR;
  }
}

// Transfer start timer callback
static void IRAM_ATTR transfer_start_timer_cb(void *arg)
{
  struct joybus *bus = (struct joybus *)arg;

  transfer_start(bus);
}

// RMT interrupt handler
static void IRAM_ATTR rmt_irq_handler(void *arg)
{
  struct joybus *bus             = (struct joybus *)arg;
  struct joybus_esp32_data *data = &JOYBUS_ESP32(bus)->data;
  uint32_t status                = RMT.int_st.val;

  // Handle TX done interrupt
  if (status & RMT_LL_EVENT_TX_DONE(data->rmt_tx_ch)) {
    rmt_ll_clear_interrupt_status(&RMT, RMT_LL_EVENT_TX_DONE(data->rmt_tx_ch));

    if (data->state == BUS_STATE_HOST_TX) {
      host_tx_complete(bus);
    } else if (data->state == BUS_STATE_TARGET_TX) {
      target_tx_complete(bus);
    }
  }

  // Handle RX threshold (byte received) interrupt
  if (status & RMT_LL_EVENT_RX_THRES(data->rmt_rx_ch)) {
    rmt_ll_clear_interrupt_status(&RMT, RMT_LL_EVENT_RX_THRES(data->rmt_rx_ch));

    // The host counts these ticks from the start of capture, during both command TX and the reply,
    // so it runs in HOST_TX and HOST_RX. The command ticks are skipped inside host_byte_received.
    if (data->state == BUS_STATE_HOST_TX || data->state == BUS_STATE_HOST_RX) {
      host_byte_received(bus);
    } else if (data->state == BUS_STATE_TARGET_RX) {
      target_byte_received(bus);
    }
  }

  // Handle TX threshold (ping-pong refill) interrupt
  if (status & RMT_LL_EVENT_TX_THRES(data->rmt_tx_ch)) {
    rmt_ll_clear_interrupt_status(&RMT, RMT_LL_EVENT_TX_THRES(data->rmt_tx_ch));

    // Refill with the next byte; after the last one, append the stop and stop refilling
    if (data->write_count < data->write_len) {
      encode_byte(data, data->write_count);
      data->write_count++;
    } else {
      encode_stop(data);
      rmt_ll_enable_interrupt(&RMT, RMT_LL_EVENT_TX_THRES(data->rmt_tx_ch), false);
    }
  }

  // Handle RX done interrupt (line went idle -> frame ended)
  if (status & RMT_LL_EVENT_RX_DONE(data->rmt_rx_ch)) {
    rmt_ll_clear_interrupt_status(&RMT, RMT_LL_EVENT_RX_DONE(data->rmt_rx_ch));

    if (data->state == BUS_STATE_TARGET_RX || data->state == BUS_STATE_TARGET_ERROR) {
      // The bus became idle without a complete command, listen for the next one
      enter_target_rx_mode(bus);
    } else if (data->state == BUS_STATE_HOST_RX) {
      // The reply frame ended before all bytes were streamed in: finish with a timeout
      transfer_finish(bus, -JOYBUS_ERR_TIMEOUT);
    }
  }
}

static void enable_rx(struct joybus *bus, uint32_t rmt_clk_freq)
{
  struct joybus_esp32_data *data = &JOYBUS_ESP32(bus)->data;

  // RMT RX idle threshold, how long the line must stay high before reception ends
  uint16_t idle_thres;
  if (bus->mode == JOYBUS_MODE_HOST) {
    // RX is armed before TX and stays on through the whole exchange, so the
    // RMT captures our command pulses as well as the response pulses as one
    // continuous capture. The turnaround time between command and response
    // reads as an idle line, so we need to set the idle threshold to the
    // longest turnaround the protocol allows. Doing this also means we get
    // response timeout detection for free from the RMT hardware.
    idle_thres = (uint16_t)((uint64_t)rmt_clk_freq * JOYBUS_REPLY_TIMEOUT_US / 1000000);
  } else {
    // A target knows a command is complete from its byte count, so completion
    // should happen inside the RX_THRES handler. We use the idle threshold
    // here to detect when the line becomes idle before a complete response
    // arrives.
    idle_thres = (rmt_clk_freq / JOYBUS_FREQ_NOMINAL) * 5 / 4;
  }

  // Reject glitches narrower than ~1/20 of a bit period
  uint16_t filter_thres = (rmt_clk_freq / JOYBUS_FREQ_NOMINAL) / 20;

  // Configure RMT RX channel
  rmt_ll_rx_set_channel_clock_div(&RMT, data->rmt_rx_ch, 1);
  rmt_ll_rx_set_mem_blocks(&RMT, data->rmt_rx_ch, 1);
  rmt_ll_rx_enable_wrap(&RMT, data->rmt_rx_ch, true);
  rmt_ll_rx_enable_carrier_demodulation(&RMT, data->rmt_rx_ch, false);
  rmt_ll_rx_set_filter_thres(&RMT, data->rmt_rx_ch, filter_thres);
  rmt_ll_rx_enable_filter(&RMT, data->rmt_rx_ch, true);
  rmt_ll_rx_set_idle_thres(&RMT, data->rmt_rx_ch, idle_thres);

  // Route the Joybus GPIO to the RMT RX channel
  esp_rom_gpio_connect_in_signal(data->gpio, JOYBUS_RMT_GROUP0.channels[data->rmt_rx_mem_ch].rx_sig, false);

  // Enable RX interrupts
  rmt_ll_clear_interrupt_status(&RMT, RMT_LL_EVENT_RX_THRES(data->rmt_rx_ch) | RMT_LL_EVENT_RX_DONE(data->rmt_rx_ch));
  rmt_ll_enable_interrupt(&RMT, RMT_LL_EVENT_RX_THRES(data->rmt_rx_ch) | RMT_LL_EVENT_RX_DONE(data->rmt_rx_ch), true);
}

static void enable_tx(struct joybus *bus, uint32_t rmt_clk_freq)
{
  struct joybus_esp32_data *data = &JOYBUS_ESP32(bus)->data;

  // Calculate Joybus TX timings
  uint32_t bit        = rmt_clk_freq / bus->freq;
  uint16_t t_short    = bit / 4;
  uint16_t t_long     = bit * 3 / 4;
  uint16_t t_stop_low = (bus->mode == JOYBUS_MODE_HOST) ? bit / 4 : bit / 2;

  // Pre-build RMT symbols
  data->sym_one  = (rmt_symbol_word_t){.duration0 = t_short, .level0 = 0, .duration1 = t_long, .level1 = 1}.val;
  data->sym_zero = (rmt_symbol_word_t){.duration0 = t_long, .level0 = 0, .duration1 = t_short, .level1 = 1}.val;
  data->sym_stop = (rmt_symbol_word_t){.duration0 = t_stop_low, .level0 = 0, .duration1 = 0, .level1 = 1}.val;

  // Configure the RMT TX channel
  rmt_ll_tx_set_channel_clock_div(&RMT, data->rmt_tx_ch, 1);
  rmt_ll_tx_set_mem_blocks(&RMT, data->rmt_tx_ch, 2);
  rmt_ll_tx_enable_loop(&RMT, data->rmt_tx_ch, false);
  rmt_ll_tx_enable_wrap(&RMT, data->rmt_tx_ch, true);
  rmt_ll_tx_set_limit(&RMT, data->rmt_tx_ch, SYMBOLS_PER_BYTE);
  rmt_ll_tx_enable_carrier_modulation(&RMT, data->rmt_tx_ch, false);
  rmt_ll_tx_fix_idle_level(&RMT, data->rmt_tx_ch, 1, true);
  rmt_ll_tx_stop(&RMT, data->rmt_tx_ch);

  // Route the TX output to the Joybus GPIO
  esp_rom_gpio_connect_out_signal(data->gpio, JOYBUS_RMT_GROUP0.channels[data->rmt_tx_ch].tx_sig, false, false);

  // Enable TX interrupts
  rmt_ll_clear_interrupt_status(&RMT, RMT_LL_EVENT_TX_DONE(data->rmt_tx_ch));
  rmt_ll_enable_interrupt(&RMT, RMT_LL_EVENT_TX_DONE(data->rmt_tx_ch), true);
}

static int joybus_esp32_enable(struct joybus *bus)
{
  struct joybus_esp32_data *data = &JOYBUS_ESP32(bus)->data;
  if (data->state != BUS_STATE_DISABLED)
    return 0;

  // Configure Joybus GPIO as a single bidirectional open-drain pin
  gpio_config_t io = {
    .pin_bit_mask = 1ULL << data->gpio,
    .mode         = GPIO_MODE_INPUT_OUTPUT_OD,
    .pull_up_en   = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type    = GPIO_INTR_DISABLE,
  };
  gpio_config(&io);

  // Resolve the response floor to CPU cycles for the busy-wait in the target reply path
  data->reply_floor_cycles = (uint32_t)TARGET_REPLY_FLOOR_NS * esp_rom_get_cpu_ticks_per_us() / 1000;

  // Enable the RMT bus clock and bring the peripheral out of reset
  PERIPH_RCC_ATOMIC()
  {
    rmt_ll_enable_bus_clock(0, true);
    rmt_ll_reset_register(0);
  }

  // Configure memory access
  rmt_ll_enable_mem_access_nonfifo(&RMT, true);
  rmt_ll_set_group_clock_src(&RMT, data->rmt_rx_ch, RMT_CLK_SRC_DEFAULT, 1, 1, 0);
  rmt_ll_enable_group_clock(&RMT, true);

  // Get the RMT source clock rate (ticks/sec)
  uint32_t rmt_clk_freq = 0;
  if (esp_clk_tree_src_get_freq_hz((soc_module_clk_t)RMT_CLK_SRC_DEFAULT, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED,
                                   &rmt_clk_freq) != ESP_OK)
    return -JOYBUS_ERR_NOT_SUPPORTED;

  // Enable RX/TX channels
  enable_rx(bus, rmt_clk_freq);
  enable_tx(bus, rmt_clk_freq);

  // Allocate the RMT interrupt handler
  if (esp_intr_alloc(JOYBUS_RMT_GROUP0.irq, ESP_INTR_FLAG_LEVEL3, rmt_irq_handler, bus, &data->rmt_intr) != 0)
    return -JOYBUS_ERR_NOT_SUPPORTED;

  // Setup transfer start timer
  esp_timer_create_args_t transfer_start_args = {
    .callback        = transfer_start_timer_cb,
    .arg             = bus,
    .dispatch_method = TIMER_DISPATCH,
    .name            = "joybus_transfer_start",
  };

  if (esp_timer_create(&transfer_start_args, &data->transfer_start_timer) != 0)
    return -JOYBUS_ERR_NOT_SUPPORTED;

  // Enter the appropriate initial state based on the bus mode
  if (bus->mode == JOYBUS_MODE_TARGET) {
    enter_target_rx_mode(bus);
  } else {
    data->state = BUS_STATE_HOST_IDLE;
  }

  return 0;
}

static int joybus_esp32_disable(struct joybus *bus)
{
  struct joybus_esp32_data *data = &JOYBUS_ESP32(bus)->data;
  if (data->state == BUS_STATE_DISABLED)
    return 0;

  // Tear down RMT
  rmt_ll_rx_enable(&RMT, data->rmt_rx_ch, false);
  rmt_ll_enable_interrupt(&RMT, UINT32_MAX, false);

  // Free any interrupt resources
  if (data->rmt_intr) {
    esp_intr_free(data->rmt_intr);
    data->rmt_intr = NULL;
  }

  // Stop and free timer resources
  if (data->transfer_start_timer) {
    esp_timer_stop(data->transfer_start_timer);
    esp_timer_delete(data->transfer_start_timer);
    data->transfer_start_timer = NULL;
  }

  data->state = BUS_STATE_DISABLED;

  return 0;
}

static int joybus_esp32_transfer(struct joybus *bus, const uint8_t *write_buf, uint8_t write_len, uint8_t *read_buf,
                                 uint8_t read_len, joybus_transfer_cb callback, void *user_data)
{
  struct joybus_esp32_data *data = &JOYBUS_ESP32(bus)->data;

  if (data->state == BUS_STATE_DISABLED)
    return -JOYBUS_ERR_DISABLED;

  if (data->state != BUS_STATE_HOST_IDLE)
    return -JOYBUS_ERR_BUSY;

  // Save the transfer context
  data->write_buf      = write_buf;
  data->write_len      = write_len;
  data->read_buf       = read_buf;
  data->read_len       = read_len;
  data->read_count     = 0;
  data->done_callback  = callback;
  data->done_user_data = user_data;

  int64_t wait_us = (int64_t)JOYBUS_INTER_TRANSFER_DELAY_US - (esp_timer_get_time() - data->last_transfer_us);
  if (wait_us <= 0) {
    // Kick off the transfer immediately if we are not in an inter-transfer delay
    transfer_start(bus);
  } else {
    // Otherwise, set up the timer to kick off the transfer later
    data->state = BUS_STATE_HOST_TX_WAIT;
    esp_timer_start_once(data->transfer_start_timer, (uint64_t)wait_us);
  }

  return 0;
}

static const struct joybus_api esp32_api = {
  .enable   = joybus_esp32_enable,
  .disable  = joybus_esp32_disable,
  .transfer = joybus_esp32_transfer,
};

int joybus_esp32_init(struct joybus_esp32 *esp32_bus, struct joybus_esp32_config config)
{
  // Save the bus API and common configuration
  struct joybus *bus = JOYBUS(esp32_bus);
  bus->api           = &esp32_api;
  bus->target        = NULL;
  bus->freq          = config.freq;

  // Save the ESP32-specific configuration and initialize state
  struct joybus_esp32_data *data = &esp32_bus->data;
  data->state                    = BUS_STATE_DISABLED;
  data->gpio                     = config.gpio;
  data->rmt_tx_ch                = config.rmt_tx_ch;
  data->rmt_rx_ch                = config.rmt_rx_ch;
  data->rmt_rx_mem_ch            = config.rmt_rx_ch + (JOYBUS_RMT_CHANNELS_PER_GROUP - JOYBUS_RMT_TX_CANDIDATES);
  data->rmt_intr                 = NULL;
  data->transfer_start_timer     = NULL;
  data->last_transfer_us         = 0;

  return 0;
}
