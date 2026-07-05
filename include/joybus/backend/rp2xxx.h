/**
 * @defgroup joybus_backend_rp2xxx RP2xxx Backend
 * @ingroup joybus_backends
 *
 * Raspberry Pi RP2040/RP2350 Joybus backend
 *
 * @{
 */

#pragma once

#include <hardware/pio.h>
#include <pico/time.h>

#include <joybus/bus.h>

/**
 * Macro to cast a generic Joybus instance to a RP2xxx Joybus instance.
 */
#define JOYBUS_RP2XXX(bus) ((struct joybus_rp2xxx *)(bus))

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
  alarm_id_t rx_timeout_alarm;

  // Transfer state
  joybus_transfer_cb done_callback;
  void *done_user_data;
  absolute_time_t last_transfer_time;
  alarm_id_t transfer_start_alarm;
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

  /// Transmit frequency, in Hz
  uint32_t freq;
};

/**
 * Build a RP2xxx config with default values.
 *
 * @param gpio the GPIO pin to use for the Joybus data line
 * @return a config with the given GPIO, the pio0 instance, and a nominal frequency
 */
static inline struct joybus_rp2xxx_config joybus_rp2xxx_config_default(uint8_t gpio)
{
  return (struct joybus_rp2xxx_config){
    .gpio = gpio,
    .pio  = pio0,
    .freq = JOYBUS_FREQ_NOMINAL,
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
