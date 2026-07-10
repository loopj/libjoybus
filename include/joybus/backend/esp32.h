/**
 * @defgroup joybus_backend_esp32 ESP32 Backend
 * @ingroup joybus_backends
 *
 * Espressif ESP32 Joybus backend.
 *
 * @{
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <esp_intr_alloc.h>
#include <esp_timer.h>
#include <driver/gpio.h>

#include <joybus/bus.h>

/**
 * Macro to cast a generic Joybus instance to an ESP32 Joybus instance.
 */
#define JOYBUS_ESP32(bus) ((struct joybus_esp32 *)(bus))

// Private implementation details - do not access directly
struct joybus_esp32_data {
  // Bus state
  uint8_t state;

  // GPIO configuration
  gpio_num_t gpio;

  // RMT channel assignment
  uint8_t rmt_tx_ch;
  uint8_t rmt_rx_ch;
  uint8_t rmt_rx_mem_ch;

  // RMT interrupt
  intr_handle_t rmt_intr;

  // Pre-encoded TX symbols
  uint32_t sym_one;
  uint32_t sym_zero;
  uint32_t sym_stop;

  // RX/TX state
  uint8_t *read_buf;
  uint8_t read_len;
  uint8_t read_count;
  uint8_t rx_skip;
  const uint8_t *write_buf;
  uint8_t write_len;
  uint8_t write_count;

  // One-shot timer for inter-transfer gap scheduling
  esp_timer_handle_t transfer_start_timer;

  // Transfer state
  joybus_transfer_cb done_callback;
  void *done_user_data;
  int64_t last_transfer_us;
};

/**
 * An ESP32 Joybus instance.
 */
struct joybus_esp32 {
  struct joybus base;
  struct joybus_esp32_data data;
};

/**
 * Configuration for an ESP32 Joybus instance.
 */
struct joybus_esp32_config {
  /// GPIO pin to use for the Joybus data line (single bidirectional open-drain).
  gpio_num_t gpio;

  /// Transmit frequency, in Hz.
  uint32_t freq;

  /// RMT TX channel index (0-based among the chip's TX-capable channels).
  uint8_t rmt_tx_ch;

  /// RMT RX channel index (0-based among the chip's RX-capable channels).
  uint8_t rmt_rx_ch;
};

/**
 * Build an ESP32 config with default values.
 *
 * @param gpio the GPIO pin to use for the Joybus data line
 * @return a config with the given GPIO and a nominal frequency
 */
static inline struct joybus_esp32_config joybus_esp32_config_default(gpio_num_t gpio)
{
  return (struct joybus_esp32_config){
    .gpio      = gpio,
    .freq      = JOYBUS_FREQ_NOMINAL,
    .rmt_tx_ch = 0,
    .rmt_rx_ch = 0,
  };
}

/**
 * Initialize an ESP32 Joybus instance.
 *
 * @param esp32_bus the ESP32 Joybus instance to initialize
 * @param config the configuration to use, eg. from joybus_esp32_config_default()
 * @return 0 on success, a negative joybus_error on failure
 */
int joybus_esp32_init(struct joybus_esp32 *esp32_bus, struct joybus_esp32_config config);

/** @} */
