/**
 * Raspberry Pi RP2040/RP2350 Joybus backend
 *
 * @defgroup joybus_backend_rp2xxx RP2xxx Backend
 * @ingroup joybus_backends
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

  // Bus frequencies
  uint32_t target_freq;
  uint32_t host_freq;

  // GPIO configuration
  uint gpio;

  // PIO instance and state machine
  PIO pio;
  uint pio_sm;
  uint8_t pio_sm_mode;

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
  joybus_transfer_cb_t done_callback;
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
 * Initialize a RP2xxx Joybus instance.
 *
 * @param rp2xxx_bus the RP2xxx Joybus instance to initialize
 * @param gpio the GPIO pin to use for the Joybus data line
 * @param pio the PIO instance to use (eg. pio0 or pio1)
 * @return 0 on success, negative error code on failure
 */
int joybus_rp2xxx_init(struct joybus_rp2xxx *rp2xxx_bus, uint8_t gpio, PIO pio);

/** @} */