#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <hardware/sync.h>
#include <hardware/timer.h>

#include <joybus/bus.h>
#include <joybus/errors.h>
#include <joybus/target.h>
#include <joybus/backend/rp2xxx.h>

#include "joybus_host.pio.h"
#include "joybus_target.pio.h"

enum {
  BUS_STATE_DISABLED,
  BUS_STATE_HOST_IDLE,
  BUS_STATE_HOST_TX,
  BUS_STATE_HOST_RX,
  BUS_STATE_TARGET_RX,
  BUS_STATE_TARGET_TX,
};

// The first bus waiting on each alarm, with the rest chained behind it
static struct joybus *alarm_buses[NUM_GENERIC_TIMERS][NUM_ALARMS];

// Arm an alarm for the earliest deadline its buses are still waiting on
static void alarm_rearm(timer_hw_t *timer, uint alarm_num)
{
  uint timer_num    = timer_get_index(timer);
  uint64_t earliest = UINT64_MAX;

  // Scheduling runs in both interrupt and thread context, so this stays atomic
  uint32_t save = save_and_disable_interrupts();

  // Find the soonest deadline any bus on this alarm is still waiting for
  for (struct joybus *bus = alarm_buses[timer_num][alarm_num]; bus != NULL;) {
    struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

    if (data->alarm_callback != NULL && data->alarm_target_us <= earliest)
      earliest = data->alarm_target_us;

    bus = data->alarm_next;
  }

  // Nothing is waiting, so leave the alarm free
  if (earliest == UINT64_MAX) {
    timer_hardware_alarm_cancel(timer, alarm_num);
    restore_interrupts(save);

    return;
  }

  absolute_time_t target;
  update_us_since_boot(&target, earliest);

  // A target already behind the counter never matches, so raise it by hand
  if (timer_hardware_alarm_set_target(timer, alarm_num, target))
    timer_hardware_alarm_force_irq(timer, alarm_num);

  restore_interrupts(save);
}

// Hand an expired alarm to every bus it is due for, then arm it for the rest
static inline void alarm_dispatch(uint timer_num, uint alarm_num)
{
  uint64_t now = time_us_64();

  // Call back every bus on this alarm whose deadline has passed
  for (struct joybus *bus = alarm_buses[timer_num][alarm_num]; bus != NULL;) {
    struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

    // Taken before the callback runs, since it may schedule the next timeout
    struct joybus *next = data->alarm_next;

    if (data->alarm_callback != NULL && data->alarm_target_us <= now) {
      void (*callback)(void *user_data) = data->alarm_callback;
      data->alarm_callback              = NULL;
      callback(bus);
    }

    bus = next;
  }

  alarm_rearm(timer_get_instance(timer_num), alarm_num);
}

// Alarm callback, which finds its timer from the vector it arrived on
static void alarm_fired(uint alarm_num)
{
  alarm_dispatch(TIMER_NUM_FROM_IRQ(__get_current_exception() - VTABLE_FIRST_IRQ), alarm_num);
}

// Schedule the bus's timeout, replacing anything already scheduled
static inline void alarm_set(struct joybus *bus, uint32_t delay_us, void (*callback)(void *user_data))
{
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  // The deadline is two stores wide, so the alarm cannot read it half written
  uint32_t save         = save_and_disable_interrupts();
  data->alarm_target_us = time_us_64() + delay_us;
  data->alarm_callback  = callback;
  restore_interrupts(save);

  alarm_rearm(data->timer, data->alarm_num);
}

// Cancel the bus's timeout, leaving the alarm to whatever else is waiting
static inline void alarm_cancel(struct joybus *bus)
{
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  data->alarm_callback = NULL;

  alarm_rearm(data->timer, data->alarm_num);
}

// Global state to track loaded PIO programs and bus instances
static struct {
  uint host_offset;
  uint target_offset;
  uint8_t ref_count;
  struct joybus *bus_instances[NUM_PIO_STATE_MACHINES];
} pio_state[NUM_PIOS] = {0};

// Load the PIO program for the bus mode
static void configure_state_machine(struct joybus *bus)
{
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  // Return early if the program is already loaded
  if (data->pio_configured)
    return;

  // Disable current state machine
  pio_sm_set_enabled(data->pio, data->pio_sm, false);

  // Initialize the PIO program
  if (bus->mode == JOYBUS_MODE_HOST) {
    joybus_host_program_init(data->pio, data->pio_sm, pio_state[PIO_NUM(data->pio)].host_offset, data->gpio, bus->freq);
  } else {
    joybus_target_program_init(data->pio, data->pio_sm, pio_state[PIO_NUM(data->pio)].target_offset, data->gpio,
                               bus->freq);
  }

  data->pio_configured = true;
}

// Enter either host idle mode or target read mode, depending on the bus mode
static inline void enter_idle_mode(struct joybus *bus, bool await_idle)
{
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  if (bus->mode == JOYBUS_MODE_TARGET) {
    // Wait for bus idle
    if (await_idle) {
      uint64_t high_since = time_us_64();
      while (time_us_64() - high_since < JOYBUS_BUS_IDLE_US) {
        if (!gpio_get(data->gpio)) {
          high_since = time_us_64();
        }
      }
    }

    // Reset read state, and forget any response from the last command
    data->read_buf   = bus->command_buffer;
    data->read_len   = JOYBUS_BLOCK_SIZE;
    data->read_count = 0;
    data->write_len  = 0;

    // Make sure the PIO program is loaded
    configure_state_machine(bus);

    // Restart the state machine
    // TODO: Consider performing the state machine reset only when strictly needed
    pio_sm_set_enabled(data->pio, data->pio_sm, false);
    pio_sm_clear_fifos(data->pio, data->pio_sm);
    dma_channel_abort(data->dma_chan_tx);
    pio_sm_restart(data->pio, data->pio_sm);
    pio_sm_exec(data->pio, data->pio_sm, pio_encode_jmp(pio_state[PIO_NUM(data->pio)].target_offset));
    pio_sm_set_enabled(data->pio, data->pio_sm, true);

    // Transition state
    data->state = BUS_STATE_TARGET_RX;
  } else {
    // Make sure the PIO program is loaded
    configure_state_machine(bus);

    // Enter host idle mode
    // TODO: Consider performing the state machine reset only when strictly needed
    pio_sm_set_enabled(data->pio, data->pio_sm, false);
    dma_channel_abort(data->dma_chan_rx);
    pio_sm_restart(data->pio, data->pio_sm);
    pio_sm_exec(data->pio, data->pio_sm,
                pio_encode_jmp(pio_state[PIO_NUM(data->pio)].host_offset + joybus_host_offset_transmit));
    pio_sm_set_enabled(data->pio, data->pio_sm, true);

    // Transition state
    data->state = BUS_STATE_HOST_IDLE;
  }
}

static inline void handle_command_response(const uint8_t *buffer, uint8_t length, void *user_data)
{
  struct joybus *bus              = (struct joybus *)user_data;
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  // Arm the DMA transfer as soon as we have a response
  data->write_len = length;
  dma_channel_set_read_addr(data->dma_chan_tx, (const void *)buffer, false);
  dma_channel_set_transfer_count(data->dma_chan_tx, length, false);
  dma_channel_start(data->dma_chan_tx);
}

// Start a pre-armed transfer
static void transfer_start(void *user_data)
{
  struct joybus *bus              = (struct joybus *)user_data;
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  // Kick off the TX DMA channel to send the command
  dma_channel_set_read_addr(data->dma_chan_tx, (const void *)data->write_buf, false);
  dma_channel_set_transfer_count(data->dma_chan_tx, data->write_len, true);

  // Arm the RX DMA channel to receive the response
  dma_channel_set_write_addr(data->dma_chan_rx, (void *)data->read_buf, false);
  dma_channel_set_transfer_count(data->dma_chan_rx, data->read_len, true);
}

// Handle transfer timeouts
static void transfer_timeout(void *user_data)
{
  struct joybus *bus              = (struct joybus *)user_data;
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  // Timeout occurred, switch back to idle/read mode
  enter_idle_mode(bus, true);

  // Record the completion time for enforcing minimum delay between transfers
  data->last_transfer_us = time_us_64();

  // Call the transfer complete callback with an error
  if (data->done_callback)
    data->done_callback(bus, -JOYBUS_ERR_TIMEOUT, data->done_user_data);
}

// Handle target rx byte timeouts
static void target_rx_timeout(void *user_data)
{
  struct joybus *bus = (struct joybus *)user_data;

  // Timeout occurred, switch back to idle/read mode
  enter_idle_mode(bus, true);
}

// Handle host tx complete (all command bytes sent)
static inline void host_tx_complete(struct joybus *bus)
{
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  // Add a timeout alarm
  alarm_set(bus, JOYBUS_REPLY_TIMEOUT_US, transfer_timeout);

  // Update the state machine for the next interrupt
  data->state = BUS_STATE_HOST_RX;
}

// Handle host byte received
static inline void host_byte_received(struct joybus *bus)
{
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  // Cancel the transfer timeout
  alarm_cancel(bus);

  // Track the received byte
  data->read_count++;

  if (data->read_count < data->read_len) {
    // Set a new timeout for the next byte
    alarm_set(bus, JOYBUS_REPLY_TIMEOUT_US, transfer_timeout);
  } else if (data->read_count == data->read_len) {
    // All bytes received, switch back to idle/read mode
    enter_idle_mode(bus, false);

    // Record the completion time for enforcing minimum interval between transfers
    data->last_transfer_us = time_us_64();

    // Call the transfer complete callback with a success status
    if (data->done_callback)
      data->done_callback(bus, 0, data->done_user_data);
  }
}

// Handle target byte received
static inline void target_byte_received(struct joybus *bus)
{
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  // Cancel the transfer timeout (only armed after the first byte)
  if (data->read_count > 0)
    alarm_cancel(bus);

  // Save the received byte in the buffer
  data->read_buf[data->read_count] = pio_sm_get(data->pio, data->pio_sm) & 0xFF;
  data->read_count++;

  // Call the target handler to prepare a response if needed
  int rc = joybus_byte_received(bus, data->read_buf, data->read_count, handle_command_response, bus);
  if (rc == 0 && data->write_len > 0) {
    // No more bytes expected, start transmitting the response
    pio_sm_exec(data->pio, data->pio_sm,
                pio_encode_jmp(pio_state[PIO_NUM(data->pio)].target_offset + joybus_target_offset_transmit));

    data->state = BUS_STATE_TARGET_TX;
  } else if (rc == 0) {
    // No more bytes expected and no response to send, switch back to idle/read mode
    enter_idle_mode(bus, true);
  } else if (rc > 0) {
    // More bytes expected
    // Set a timeout for the next byte
    alarm_set(bus, JOYBUS_REPLY_TIMEOUT_US, target_rx_timeout);
  } else {
    // Error handling command, or command not supported, switch back to idle/read mode
    enter_idle_mode(bus, true);
  }
}

// Handle target tx complete (all response bytes sent)
static inline void target_tx_complete(struct joybus *bus)
{
  // Switch back to idle/read mode
  enter_idle_mode(bus, false);
}

// PIO IRQ handler
static void __isr __not_in_flash_func(pio_irq_handler)(void)
{
  // Determine which PIO instance triggered the interrupt
  uint pio_num = (__get_current_exception() - VTABLE_FIRST_IRQ - PIO0_IRQ_0) / 2;
  PIO pio      = PIO_INSTANCE(pio_num);

  // Get and clear pending interrupts
  uint32_t pending = pio->irq;
  pio->irq         = pending;

  // Service each state machine that triggered an interrupt
  while (pending) {
    int sm = __builtin_ctz(pending);
    pending &= ~(1 << sm);

    // Lookup the bus instance
    struct joybus *bus = pio_state[pio_num].bus_instances[sm];
    if (!bus)
      continue;

    // Handle the interrupt based on the bus state
    struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;
    switch (data->state) {
      case BUS_STATE_HOST_TX:
        host_tx_complete(bus);
        break;
      case BUS_STATE_HOST_RX:
        host_byte_received(bus);
        break;
      case BUS_STATE_TARGET_RX:
        target_byte_received(bus);
        break;
      case BUS_STATE_TARGET_TX:
        target_tx_complete(bus);
        break;
      default:
        break;
    }
  }
}

static int joybus_rp2xxx_enable(struct joybus *bus)
{
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;
  if (data->state != BUS_STATE_DISABLED)
    return 0;

  // Claim a state machine
  data->pio_sm = pio_claim_unused_sm(data->pio, true);

  // Load PIO programs if not already loaded
  uint pio_num = PIO_NUM(data->pio);
  if (pio_state[pio_num].ref_count == 0) {
    pio_state[pio_num].host_offset   = pio_add_program(data->pio, &joybus_host_program);
    pio_state[pio_num].target_offset = pio_add_program(data->pio, &joybus_target_program);
  }
  pio_state[pio_num].ref_count++;

  // Map the PIO instance to this bus for interrupt handling
  pio_state[pio_num].bus_instances[data->pio_sm] = bus;

  // Initialize the GPIO
  pio_gpio_init(data->pio, data->gpio);

  // Enable PIO IRQ handler
  irq_set_exclusive_handler(PIO_IRQ_NUM(data->pio, 0), pio_irq_handler);
  irq_set_enabled(PIO_IRQ_NUM(data->pio, 0), true);
  pio_set_irq0_source_enabled(data->pio, pis_interrupt0 + data->pio_sm, true);

  // Join the alarm's buses, claiming it if this is the first one on it
  uint timer_num = timer_get_index(data->timer);
  if (alarm_buses[timer_num][data->alarm_num] == NULL) {
    timer_hardware_alarm_claim(data->timer, data->alarm_num);
    timer_hardware_alarm_set_callback(data->timer, data->alarm_num, alarm_fired);
  }
  data->alarm_next                        = alarm_buses[timer_num][data->alarm_num];
  alarm_buses[timer_num][data->alarm_num] = bus;

  // Allocate DMA channels
  data->dma_chan_tx = dma_claim_unused_channel(true);
  data->dma_chan_rx = dma_claim_unused_channel(true);

  // Configure TX DMA to write to TX FIFO
  dma_channel_config dma_config_tx = dma_channel_get_default_config(data->dma_chan_tx);
  channel_config_set_transfer_data_size(&dma_config_tx, DMA_SIZE_8);
  channel_config_set_read_increment(&dma_config_tx, true);
  channel_config_set_write_increment(&dma_config_tx, false);
  channel_config_set_dreq(&dma_config_tx, PIO_DREQ_NUM(data->pio, data->pio_sm, true));
  dma_channel_set_config(data->dma_chan_tx, &dma_config_tx, false);

  // Use the MSB of the TX FIFO for 8-bit writes
  io_rw_8 *txf_msb = (io_rw_8 *)&data->pio->txf[data->pio_sm] + 3;
  dma_channel_set_write_addr(data->dma_chan_tx, (void *)txf_msb, false);

  // Configure DMA for RX
  dma_channel_config dma_config_rx = dma_channel_get_default_config(data->dma_chan_rx);
  channel_config_set_transfer_data_size(&dma_config_rx, DMA_SIZE_8);
  channel_config_set_read_increment(&dma_config_rx, false);
  channel_config_set_write_increment(&dma_config_rx, true);
  channel_config_set_dreq(&dma_config_rx, PIO_DREQ_NUM(data->pio, data->pio_sm, false));
  dma_channel_set_config(data->dma_chan_rx, &dma_config_rx, false);
  dma_channel_set_read_addr(data->dma_chan_rx, &data->pio->rxf[data->pio_sm], false);

  // Start in the appropriate mode
  enter_idle_mode(bus, true);

  return 0;
}

static int joybus_rp2xxx_disable(struct joybus *bus)
{
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;
  if (data->state == BUS_STATE_DISABLED)
    return 0;

  // Stop the state machine and give it back
  pio_sm_set_enabled(data->pio, data->pio_sm, false);
  pio_set_irq0_source_enabled(data->pio, pis_interrupt0 + data->pio_sm, false);
  pio_sm_unclaim(data->pio, data->pio_sm);

  // Unload the programs once the last bus on this instance has gone
  uint pio_num                                   = PIO_NUM(data->pio);
  pio_state[pio_num].bus_instances[data->pio_sm] = NULL;
  if (--pio_state[pio_num].ref_count == 0) {
    pio_remove_program(data->pio, &joybus_host_program, pio_state[pio_num].host_offset);
    pio_remove_program(data->pio, &joybus_target_program, pio_state[pio_num].target_offset);
  }
  data->pio_configured = false;

  // Give the DMA channels back
  dma_channel_abort(data->dma_chan_tx);
  dma_channel_abort(data->dma_chan_rx);
  dma_channel_unclaim(data->dma_chan_tx);
  dma_channel_unclaim(data->dma_chan_rx);

  // Leave the alarm's buses, releasing it with the last one
  alarm_cancel(bus);

  uint timer_num       = timer_get_index(data->timer);
  struct joybus **link = &alarm_buses[timer_num][data->alarm_num];
  while (*link != NULL) {
    if (*link == bus) {
      *link = data->alarm_next;
      break;
    }

    link = &JOYBUS_RP2XXX(*link)->data.alarm_next;
  }

  if (alarm_buses[timer_num][data->alarm_num] == NULL) {
    timer_hardware_alarm_set_callback(data->timer, data->alarm_num, NULL);
    timer_hardware_alarm_unclaim(data->timer, data->alarm_num);
  }

  data->state = BUS_STATE_DISABLED;

  return 0;
}

static int joybus_rp2xxx_transfer(struct joybus *bus, const uint8_t *write_buf, uint8_t write_len, uint8_t *read_buf,
                                  uint8_t read_len, joybus_transfer_cb callback, void *user_data)
{
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  if (data->state == BUS_STATE_DISABLED)
    return -JOYBUS_ERR_DISABLED;

  if (data->state != BUS_STATE_HOST_IDLE)
    return -JOYBUS_ERR_BUSY;

  // Save the transfer context
  data->write_buf      = (uint8_t *)write_buf;
  data->write_len      = write_len;
  data->read_buf       = read_buf;
  data->read_len       = read_len;
  data->read_count     = 0;
  data->done_callback  = callback;
  data->done_user_data = user_data;

  // Mark transfer as started
  data->state = BUS_STATE_HOST_TX;

  // Schedule the transfer to start at last_completion + the minimum intra-transfer delay
  // If the time has already passed, the callback fires immediately
  uint64_t now      = time_us_64();
  uint64_t ready_at = data->last_transfer_us + JOYBUS_INTER_TRANSFER_DELAY_US;
  uint32_t wait_us  = ready_at > now ? (uint32_t)(ready_at - now) : 0;
  alarm_set(bus, wait_us, transfer_start);

  return 0;
}

static const struct joybus_api rp2xxx_api = {
  .enable   = joybus_rp2xxx_enable,
  .disable  = joybus_rp2xxx_disable,
  .transfer = joybus_rp2xxx_transfer,
};

int joybus_rp2xxx_init(struct joybus_rp2xxx *rp2xxx_bus, struct joybus_rp2xxx_config config)
{
  if (config.timer == NULL || config.alarm_num >= NUM_ALARMS)
    return -JOYBUS_ERR_INVALID_ARG;

  // Save the bus API
  struct joybus *bus = JOYBUS(rp2xxx_bus);
  bus->api           = &rp2xxx_api;
  bus->freq          = config.freq;
  bus->targets       = NULL;
  bus->active_target = NULL;

  // Save the joybus configuration
  struct joybus_rp2xxx_data *data = &rp2xxx_bus->data;
  data->gpio                      = config.gpio;
  data->pio                       = config.pio;
  data->timer                     = config.timer;
  data->alarm_num                 = config.alarm_num;
  data->pio_configured            = false;
  data->state                     = BUS_STATE_DISABLED;
  data->last_transfer_us          = 0;

  return 0;
}
