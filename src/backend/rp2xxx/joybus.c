#include <stdio.h>

#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/pio.h>
#include <pico/stdlib.h>

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

enum {
  BUS_MODE_NONE,
  BUS_MODE_HOST,
  BUS_MODE_TARGET,
};

// Global state to track loaded PIO programs and bus instances
static struct {
  uint host_offset;
  uint target_offset;
  uint8_t ref_count;
  struct joybus *bus_instances[NUM_PIO_STATE_MACHINES];
} pio_state[NUM_PIOS] = {0};

// Configure the state machine for host or target mode
static void configure_state_machine(struct joybus *bus, int mode)
{
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  // Return early if already in the requested mode
  if (data->pio_sm_mode == mode)
    return;

  // Disable current state machine
  pio_sm_set_enabled(data->pio, data->pio_sm, false);

  // Initialize the PIO program
  if (mode == BUS_MODE_HOST) {
    joybus_host_program_init(data->pio, data->pio_sm, pio_state[PIO_NUM(data->pio)].host_offset, data->gpio,
                             data->host_freq);
  } else {
    joybus_target_program_init(data->pio, data->pio_sm, pio_state[PIO_NUM(data->pio)].target_offset, data->gpio,
                               data->target_freq);
  }

  // Update the mode
  data->pio_sm_mode = mode;
}

// Enter either host idle mode or target read mode, depending on whether a target is registered
static inline void enter_idle_mode(struct joybus *bus, bool await_bus_idle)
{
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  if (bus->target) {
    // TODO: Wait for bus idle if requested

    // Reset read state
    data->read_buf   = bus->command_buffer;
    data->read_len   = JOYBUS_BLOCK_SIZE;
    data->read_count = 0;

    // Make sure the state machine is in target mode
    configure_state_machine(bus, BUS_MODE_TARGET);

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
    // Make sure the state machine is in host mode
    configure_state_machine(bus, BUS_MODE_HOST);

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
  dma_channel_set_read_addr(data->dma_chan_tx, (const void *)buffer, false);
  dma_channel_set_transfer_count(data->dma_chan_tx, length, false);
  dma_channel_start(data->dma_chan_tx);
}

// Start a pre-armed transfer
int64_t transfer_start(alarm_id_t id, void *user_data)
{
  struct joybus *bus              = (struct joybus *)user_data;
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  // Kick off the TX DMA channel to send the command
  dma_channel_set_read_addr(data->dma_chan_tx, (const void *)data->write_buf, false);
  dma_channel_set_transfer_count(data->dma_chan_tx, data->write_len, true);

  // Arm the RX DMA channel to receive the response
  dma_channel_set_write_addr(data->dma_chan_rx, (void *)data->read_buf, false);
  dma_channel_set_transfer_count(data->dma_chan_rx, data->read_len, true);

  return 0;
}

// Handle transfer timeouts
int64_t transfer_timeout(alarm_id_t id, void *user_data)
{
  struct joybus *bus              = (struct joybus *)user_data;
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  // Timeout occurred, switch back to idle/read mode
  enter_idle_mode(bus, true);

  // Record the completion time for enforcing minimum delay between transfers
  data->last_transfer_time = get_absolute_time();

  // Call the transfer complete callback with an error
  if (data->done_callback)
    data->done_callback(bus, -JOYBUS_ERR_TIMEOUT, data->done_user_data);

  return 0;
}

// Handle target rx byte timeouts
int64_t target_rx_timeout(alarm_id_t id, void *user_data)
{
  struct joybus *bus              = (struct joybus *)user_data;
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  // Timeout occurred, switch back to idle/read mode
  enter_idle_mode(bus, true);

  return 0;
}

// Handle host tx complete (all command bytes sent)
static inline void host_tx_complete(struct joybus *bus)
{
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  // Add a timeout alarm
  data->rx_timeout_alarm = add_alarm_in_us(JOYBUS_REPLY_TIMEOUT_US, transfer_timeout, bus, true);

  // Update the state machine for the next interrupt
  data->state = BUS_STATE_HOST_RX;
}

// Handle host byte received
static inline void host_byte_received(struct joybus *bus)
{
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  // Cancel the transfer timeout
  cancel_alarm(data->rx_timeout_alarm);

  // Track the received byte
  data->read_count++;

  if (data->read_count < data->read_len) {
    // Set a new timeout for the next byte
    data->rx_timeout_alarm = add_alarm_in_us(JOYBUS_REPLY_TIMEOUT_US, transfer_timeout, bus, true);
  } else if (data->read_count == data->read_len) {
    // All bytes received, switch back to idle/read mode
    enter_idle_mode(bus, false);

    // Record the completion time for enforcing minimum interval between transfers
    data->last_transfer_time = get_absolute_time();

    // Call the transfer complete callback
    if (data->done_callback)
      data->done_callback(bus, data->read_len, data->done_user_data);
  }
}

// Handle target byte received
static inline void target_byte_received(struct joybus *bus)
{
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  // Cancel the transfer timeout
  cancel_alarm(data->rx_timeout_alarm);

  // Save the received byte in the buffer
  data->read_buf[data->read_count] = pio_sm_get(data->pio, data->pio_sm) & 0xFF;
  data->read_count++;

  // Call the target handler to prepare a response if needed
  int rc = joybus_target_byte_received(bus->target, data->read_buf, data->read_count, handle_command_response, bus);
  if (rc == 0) {
    // No more bytes expected, start transmitting the response
    pio_sm_exec(data->pio, data->pio_sm,
                pio_encode_jmp(pio_state[PIO_NUM(data->pio)].target_offset + joybus_target_offset_transmit));

    data->state = BUS_STATE_TARGET_TX;
  } else if (rc > 0) {
    // More bytes expected
    // Set a timeout for the next byte
    data->rx_timeout_alarm = add_alarm_in_us(JOYBUS_REPLY_TIMEOUT_US, target_rx_timeout, bus, true);
  } else {
    // Error handling command, or command not supported, switch back to idle/read mode
    enter_idle_mode(bus, true);
  }
}

// Handle target tx complete (all response bytes sent)
static inline void target_tx_complete(struct joybus *bus)
{
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

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

  // TODO: Handle peripheral teardown (DMA channels, IRQ cleanup, etc.)

  data->state = BUS_STATE_DISABLED;

  return 0;
}

static int joybus_rp2xxx_transfer(struct joybus *bus, const uint8_t *write_buf, uint8_t write_len, uint8_t *read_buf,
                                  uint8_t read_len, joybus_transfer_cb_t callback, void *user_data)
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
  absolute_time_t ready_time = delayed_by_us(data->last_transfer_time, JOYBUS_INTER_TRANSFER_DELAY_US);
  data->transfer_start_alarm = add_alarm_at(ready_time, transfer_start, bus, true);

  return 0;
}

static int joybus_rp2xxx_target_register(struct joybus *bus, struct joybus_target *target)
{
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  // Save the target
  bus->target = target;

  // Switch to target read mode if bus is enabled
  if (data->state != BUS_STATE_DISABLED)
    enter_idle_mode(bus, true);

  return 0;
}

static int joybus_rp2xxx_target_unregister(struct joybus *bus, struct joybus_target *target)
{
  struct joybus_rp2xxx_data *data = &JOYBUS_RP2XXX(bus)->data;

  // Clear the target
  bus->target = NULL;

  // Switch to host idle mode if bus is enabled
  if (data->state != BUS_STATE_DISABLED)
    enter_idle_mode(bus, false);

  return 0;
}

static const struct joybus_api rp2xxx_api = {
  .enable            = joybus_rp2xxx_enable,
  .disable           = joybus_rp2xxx_disable,
  .transfer          = joybus_rp2xxx_transfer,
  .target_register   = joybus_rp2xxx_target_register,
  .target_unregister = joybus_rp2xxx_target_unregister,
};

int joybus_rp2xxx_init(struct joybus_rp2xxx *rp2xxx_bus, uint8_t gpio, PIO pio)
{
  // Save the bus API
  struct joybus *bus = JOYBUS(rp2xxx_bus);
  bus->api           = &rp2xxx_api;
  bus->target        = NULL;

  // Save the joybus configuration
  struct joybus_rp2xxx_data *data = &rp2xxx_bus->data;
  data->gpio                      = gpio;
  data->pio                       = pio;
  data->pio_sm_mode               = BUS_MODE_NONE;
  data->state                     = BUS_STATE_DISABLED;
  data->host_freq                 = JOYBUS_FREQ_CONSOLE;
  data->target_freq               = JOYBUS_FREQ_GCC;
  data->last_transfer_time        = nil_time;

  return 0;
}