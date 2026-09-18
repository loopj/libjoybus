/**
 * @defgroup joybus_backend_rp2xxx RP2xxx Backend
 * @ingroup joybus_backends
 *
 * Raspberry Pi RP2040/RP2350 Joybus backend
 *
 * @{
 */

#pragma once

#include <hardware/irq.h>
#include <hardware/pio.h>
#include <hardware/timer.h>

#include <joybus/bus.h>

/**
 * Macro to cast a generic Joybus instance to a RP2xxx Joybus instance.
 */
#define JOYBUS_RP2XXX(bus) ((struct joybus_rp2xxx *)(bus))

struct joybus_rp2xxx;

// Private implementation details - do not access directly
struct joybus_rp2xxx_data {
  // Bus state
  uint8_t state;

  // GPIO configuration
  uint gpio;

  // PIO instance and state machine
  PIO pio;
  uint pio_sm;
  bool pio_configured;

  // DMA configuration
  uint dma_chan_tx;
  uint dma_chan_rx;

  // RX/TX state
  uint8_t *read_buf;
  uint8_t read_len;
  uint8_t read_count;
  uint8_t *write_buf;
  uint8_t write_len;
  // Hardware alarm the timeouts run on, shared with any other bus on it
  timer_hw_t *timer;
  uint alarm_num;
  struct joybus_rp2xxx *alarm_next;

  // How the PIO and alarm handlers reach their vectors
  void (*set_irq_handler)(uint irq_num, irq_handler_t handler);

  // What this bus is waiting for, if anything
  uint64_t alarm_target_us;
  void (*alarm_callback)(void *user_data);

  // Transfer state
  joybus_transfer_cb done_callback;
  void *done_user_data;
  uint64_t last_transfer_us;
};

/**
 * A RP2xxx Joybus instance.
 */
struct joybus_rp2xxx {
  struct joybus base;
  struct joybus_rp2xxx_data data;
};

/**
 * Configuration for a RP2xxx Joybus instance.
 */
struct joybus_rp2xxx_config {
  /// GPIO pin to use for the Joybus data line
  uint8_t gpio;

  /// PIO instance to use (eg. pio0 or pio1)
  PIO pio;

  /// Timer instance the reply timeouts run on
  timer_hw_t *timer;

  /// Which of that timer's alarms to claim
  uint alarm_num;

  /// Transmit frequency, in Hz
  uint32_t freq;

  /// Installs an interrupt handler and enables the line
  void (*set_irq_handler)(uint irq_num, irq_handler_t handler);
};

/**
 * Install an interrupt handler through the pico-sdk.
 *
 * @param irq_num the interrupt to take
 * @param handler what to call, which runs in interrupt context
 */
static inline void joybus_rp2xxx_set_irq_handler_sdk(uint irq_num, irq_handler_t handler)
{
  irq_set_exclusive_handler(irq_num, handler);
  irq_set_enabled(irq_num, true);
}

/**
 * Build a RP2xxx config with default values.
 *
 * An RTOS that builds its own vector table has to replace set_irq_handler.
 *
 * @param gpio the GPIO pin to use for the Joybus data line
 * @return a config with the given GPIO, the pio0 instance, and a nominal frequency
 */
static inline struct joybus_rp2xxx_config joybus_rp2xxx_config_default(uint8_t gpio)
{
  return (struct joybus_rp2xxx_config){
    .gpio            = gpio,
    .pio             = pio0,
    .timer           = PICO_DEFAULT_TIMER_INSTANCE(),
    .alarm_num       = 0,
    .freq            = JOYBUS_FREQ_NOMINAL,
    .set_irq_handler = joybus_rp2xxx_set_irq_handler_sdk,
  };
}

/**
 * Initialize a RP2xxx Joybus instance.
 *
 * @param rp2xxx_bus the RP2xxx Joybus instance to initialize
 * @param config the configuration to use, eg. from joybus_rp2xxx_config_default()
 * @return 0 on success, a negative joybus_error on failure
 */
int joybus_rp2xxx_init(struct joybus_rp2xxx *rp2xxx_bus, struct joybus_rp2xxx_config config);

/** @} */
