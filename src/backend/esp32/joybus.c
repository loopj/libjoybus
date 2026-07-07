#include <joybus/bus.h>
#include <joybus/errors.h>
#include <joybus/target.h>
#include <joybus/backend/esp32.h>

#include <esp_attr.h>
#include <esp_clk_tree.h>
#include <esp_intr_alloc.h>
#include <esp_rom_gpio.h>
#include <esp_timer.h>
#include <esp_private/periph_ctrl.h>
#include <hal/rmt_ll.h>
#include <soc/clk_tree_defs.h>
#include <soc/rmt_periph.h>
#include <soc/soc_caps.h>

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

// RX capture ring (wraps for long frames)
#define RX_RING           SOC_RMT_MEM_WORDS_PER_CHANNEL

// 2 blocks; wrap boundary for the TX refill
#define TX_MEM_SYMS       (2 * SOC_RMT_MEM_WORDS_PER_CHANNEL)

// Use lower latency interrupt dispatch method if available
#if CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD
#define TIMER_DISPATCH ESP_TIMER_ISR
#else
#define TIMER_DISPATCH ESP_TIMER_TASK
#endif

// Static check that TX_MEM_SYMS is a multiple of SYMBOLS_PER_BYTE
#if TX_MEM_SYMS % SYMBOLS_PER_BYTE != 0
#error "TX_MEM_SYMS must be a multiple of SYMBOLS_PER_BYTE"
#endif

// Direct access to RMT memory (linker-provided symbol)
extern volatile rmt_symbol_word_t RMTMEM[SOC_RMT_CHANNELS_PER_GROUP][SOC_RMT_MEM_WORDS_PER_CHANNEL];

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

  // Reset RMT TX pointer
  rmt_ll_tx_reset_pointer(&RMT, data->rmt_tx_ch);

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
 * Decode byte `byte_idx` from RMT RX memory.
 *
 * Another turnaround trick: in target mode the "byte received" interrupt can
 * fire before the byte's final symbol is committed (rx_lim is 7 for the first
 * byte, then 8 to keep each byte's interrupt landing one symbol early). So we
 * fold in each bit the moment its symbol commits. The bits already captured are
 * read while the last bit is still on the wire, and we busy-wait only for that
 * final symbol.
 */
static inline IRAM_ATTR uint8_t decode_byte(struct joybus_esp32_data *data, int byte_idx)
{
  volatile rmt_symbol_word_t *mem = RMTMEM[data->rmt_rx_mem_ch];

  // Find our position in the ring buffer
  int base = (byte_idx * SYMBOLS_PER_BYTE) % RX_RING;

  // Fill the byte
  uint8_t byte = 0;
  for (int i = 0; i < SYMBOLS_PER_BYTE;) {
    // Check if the symbol has been committed
    if (rx_committed(data, base) > i) {
      // Decode the bit and fold it into the byte
      byte = (uint8_t)((byte << 1) | decode_bit(mem[base + i]));
      i++;
    }
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

  // Configure RMT RX for a fresh frame
  // Use a 7-symbol limit for the first byte to fire the interrupt early
  rmt_ll_rx_set_limit(&RMT, data->rmt_rx_ch, SYMBOLS_PER_BYTE - 1);
  rmt_ll_rx_set_mem_owner(&RMT, data->rmt_rx_ch, RMT_LL_MEM_OWNER_HW);
  rmt_ll_rx_reset_pointer(&RMT, data->rmt_rx_ch);

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
    // Reset the byte counter
    data->read_count = 0;

    // Configure RX for a fresh frame
    rmt_ll_rx_set_limit(&RMT, data->rmt_rx_ch, SYMBOLS_PER_BYTE);
    rmt_ll_rx_set_mem_owner(&RMT, data->rmt_rx_ch, RMT_LL_MEM_OWNER_HW);
    rmt_ll_rx_reset_pointer(&RMT, data->rmt_rx_ch);
  }

  start_write(bus);

  // Transition state
  data->state = BUS_STATE_HOST_TX;
}

// Finish a host transfer and return to host idle state
static IRAM_ATTR void transfer_finish(struct joybus *bus, int status)
{
  struct joybus_esp32_data *data = &JOYBUS_ESP32(bus)->data;

  // Disable RX
  rmt_ll_rx_enable(&RMT, data->rmt_rx_ch, false);

  // Cancel rx timeout
  esp_timer_stop(data->rx_timeout_timer);

  // Record the completion time for enforcing minimum interval between transfers
  data->last_transfer_us = esp_timer_get_time();

  // Call the transfer complete callback with status
  if (data->done_callback)
    data->done_callback(bus, status, data->done_user_data);

  // Transition state
  data->state = BUS_STATE_HOST_IDLE;
}

// Handle host tx complete (all command bytes sent)
static inline IRAM_ATTR void host_tx_complete(struct joybus *bus)
{
  struct joybus_esp32_data *data = &JOYBUS_ESP32(bus)->data;

  // We've finished sending a command, check if we need to receive a response
  if (data->read_len > 0) {
    // Immediately flip into read mode
    data->state = BUS_STATE_HOST_RX;
    rmt_ll_rx_enable(&RMT, data->rmt_rx_ch, true);

    // Start the RX timeout timer
    esp_timer_start_once(data->rx_timeout_timer, JOYBUS_REPLY_TIMEOUT_US);
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

// Handle host byte received
static inline IRAM_ATTR void host_byte_received(struct joybus *bus)
{
  struct joybus_esp32_data *data = &JOYBUS_ESP32(bus)->data;

  // Process the received pulses into the byte buffer
  data->read_buf[data->read_count] = decode_byte(data, data->read_count);
  data->read_count++;

  // First byte: the reply has started, cancel the start timeout
  if (data->read_count == 1)
    esp_timer_stop(data->rx_timeout_timer);

  // Last byte
  if (data->read_count == data->read_len)
    transfer_finish(bus, 0);
}

// Handle target byte received
static inline IRAM_ATTR void target_byte_received(struct joybus *bus)
{
  struct joybus_esp32_data *data = &JOYBUS_ESP32(bus)->data;

  // Save the received byte in the buffer
  data->read_buf[data->read_count] = decode_byte(data, data->read_count);
  data->read_count++;

  // Call the target handler to prepare a response if needed
  int rc = joybus_target_byte_received(bus->target, data->read_buf, data->read_count, handle_command_response, bus);
  if (rc == 0) {
    // No more bytes expected
    // Stop input capture
    rmt_ll_rx_enable(&RMT, data->rmt_rx_ch, false);
    rmt_ll_clear_interrupt_status(&RMT, RMT_LL_EVENT_RX_DONE(data->rmt_rx_ch));

    // Start the response transfer if there is one
    if (data->write_len > 0) {
      data->state = BUS_STATE_TARGET_TX;
      rmt_ll_clear_interrupt_status(&RMT, RMT_LL_EVENT_TX_DONE(data->rmt_tx_ch));
      start_write(bus);
    } else {
      // No response to send, switch back to target read mode
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

// RX timeout timer callback
static void IRAM_ATTR rx_timeout_timer_cb(void *arg)
{
  struct joybus *bus = (struct joybus *)arg;

  transfer_finish(bus, -JOYBUS_ERR_TIMEOUT);
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

    if (data->state == BUS_STATE_HOST_RX) {
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
      // Reply ended at idle before all bytes arrived, finish with a timeout
      transfer_finish(bus, -JOYBUS_ERR_TIMEOUT);
    }
  }
}

static void enable_rx(struct joybus *bus, uint32_t rmt_clk_freq)
{
  struct joybus_esp32_data *data = &JOYBUS_ESP32(bus)->data;

  // Pick a reasonable RMT RX idle threshold based on the nominal protocol rate
  // Line idle (high) for > 5/4 of a nominal bit period during RX should end reception
  uint16_t idle_thres = (rmt_clk_freq / JOYBUS_FREQ_NOMINAL) * 5 / 4;

  // Configure RMT RX channel
  rmt_ll_rx_set_channel_clock_div(&RMT, data->rmt_rx_ch, 1);
  rmt_ll_rx_set_mem_blocks(&RMT, data->rmt_rx_ch, 1);
  rmt_ll_rx_enable_wrap(&RMT, data->rmt_rx_ch, true);
  rmt_ll_rx_enable_carrier_demodulation(&RMT, data->rmt_rx_ch, false);
  rmt_ll_rx_set_filter_thres(&RMT, data->rmt_rx_ch, 16);
  rmt_ll_rx_enable_filter(&RMT, data->rmt_rx_ch, true);
  rmt_ll_rx_set_idle_thres(&RMT, data->rmt_rx_ch, idle_thres);

  // Route the Joybus GPIO to the RMT RX channel
  esp_rom_gpio_connect_in_signal(data->gpio, rmt_periph_signals.groups[0].channels[data->rmt_rx_mem_ch].rx_sig, false);

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
  esp_rom_gpio_connect_out_signal(data->gpio, rmt_periph_signals.groups[0].channels[data->rmt_tx_ch].tx_sig, false,
                                  false);

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

  // Enable the RMT bus clock and bring the peripheral out of reset
  PERIPH_RCC_ATOMIC()
  {
    rmt_ll_enable_bus_clock(0, true);
    rmt_ll_reset_register(0);
  }

  // Configure memory access
  rmt_ll_enable_mem_access_nonfifo(&RMT, true);
  rmt_ll_set_group_clock_src(&RMT, data->rmt_rx_ch, RMT_CLK_SRC_APB, 1, 1, 0);
  rmt_ll_enable_group_clock(&RMT, true);

  // Get the RMT source clock rate (ticks/sec)
  uint32_t rmt_clk_freq = 0;
  if (esp_clk_tree_src_get_freq_hz((soc_module_clk_t)RMT_CLK_SRC_APB, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED,
                                   &rmt_clk_freq) != ESP_OK)
    return -JOYBUS_ERR_NOT_SUPPORTED;

  // Enable RX/TX channels
  enable_rx(bus, rmt_clk_freq);
  enable_tx(bus, rmt_clk_freq);

  // Allocate the RMT interrupt handler
  if (esp_intr_alloc(rmt_periph_signals.groups[0].irq, ESP_INTR_FLAG_LEVEL3, rmt_irq_handler, bus, &data->rmt_intr) !=
      0)
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

  // Setup RX timeout timer
  esp_timer_create_args_t rx_timeout_args = {
    .callback        = rx_timeout_timer_cb,
    .arg             = bus,
    .dispatch_method = TIMER_DISPATCH,
    .name            = "joybus_rx_timeout",
  };

  if (esp_timer_create(&rx_timeout_args, &data->rx_timeout_timer) != 0)
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

  if (data->rx_timeout_timer) {
    esp_timer_stop(data->rx_timeout_timer);
    esp_timer_delete(data->rx_timeout_timer);
    data->rx_timeout_timer = NULL;
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
  data->rmt_rx_mem_ch            = config.rmt_rx_ch + (SOC_RMT_CHANNELS_PER_GROUP - SOC_RMT_TX_CANDIDATES_PER_GROUP);
  data->rmt_intr                 = NULL;
  data->transfer_start_timer     = NULL;
  data->rx_timeout_timer         = NULL;
  data->last_transfer_us         = 0;

  return 0;
}
