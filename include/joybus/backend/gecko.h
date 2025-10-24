/**
 * Joybus backend for Silicon Labs EFM32/EFR32 MCUs using the Gecko SDK
 *
 * ### Configuration
 *
 * The Gecko backend is made available by setting `JOYBUS_BACKEND_GECKO=ON` in your CMake configuration.
 *
 * ### Usage
 *
 * ```c
 * #include <joybus/joybus.h>
 * #include <joybus/backend/gecko.h>
 *
 * struct joybus_gecko gecko_bus;
 * struct joybus *bus = JOYBUS(&gecko_bus);
 *
 * void main() {
 *   // Initialize and enable the bus using the Gecko backend
 *   joybus_gecko_init(&gecko_bus, gpioPortD, 3, TIMER1, USART0);
 *   joybus_enable(bus);
 *
 *   // Use the bus...
 * }
 * ```
 *
 * @defgroup joybus_backend_gecko Gecko Backend
 * @ingroup joybus_backends
 * @{
 */

#pragma once

#include "em_gpio.h"
#include "em_ldma.h"
#include "em_timer.h"
#include "em_usart.h"

#include <joybus/bus.h>

/**
 * Macro to cast a generic Joybus instance to a Gecko Joybus instance.
 */
#define JOYBUS_GECKO(bus) ((struct joybus_gecko *)(bus))

// Number of chips per bit for the line coding
#define CHIPS_PER_BIT           4

// Edges per byte for pulse timing decoding
#define EDGES_PER_BYTE          16

// Private implementation details - do not access directly
struct joybus_gecko_data {
  // Bus state
  uint8_t state;

  // Bus frequencies
  uint32_t host_freq;
  uint32_t target_freq;

  // GPIO configuration
  GPIO_Port_TypeDef gpio_port;
  uint8_t gpio_pin;

  // Peripherals
  TIMER_TypeDef *rx_timer;
  USART_TypeDef *tx_usart;

  // Transfer state
  uint8_t *read_buf;
  uint8_t read_len;
  uint8_t read_count;
  uint8_t *write_buf;
  uint8_t write_len;
  bool rx_trailing_bit;

  // Transfer callback
  joybus_transfer_cb_t done_callback;
  void *done_user_data;

  // RX timings
  uint16_t host_pulse_period_half;
  uint16_t target_pulse_period_half;
  uint16_t bus_idle_period;

  // RX ping-pong LDMA configuration
  unsigned int rx_dma_channel;
  uint16_t rx_edge_timings[2][EDGES_PER_BYTE + 2];
  LDMA_TransferCfg_t rx_config;
  LDMA_Descriptor_t rx_descriptors[2];
  uint8_t rx_current_buffer;

  // TX ping-pong LDMA configuration
  unsigned int tx_dma_channel;
  uint8_t tx_encoded_bytes[2][CHIPS_PER_BIT];
  LDMA_TransferCfg_t tx_config;
  LDMA_Descriptor_t tx_descriptors[3];
  uint8_t tx_initial_buffer;
  uint8_t tx_current_buffer;
  uint8_t tx_buffered_bytes;
};

/**
 * A Gecko Joybus instance.
 */
struct joybus_gecko {
  struct joybus base;
  struct joybus_gecko_data data;
};

/**
 * Initialize a Gecko Joybus instance.
 *
 * Note: Some peripherals cannot be used on certain ports, check the DBUS
 * Routing Table in the reference manual for your MCU.
 *
 * @param gecko_bus the Gecko Joybus instance to initialize
 * @param port the GPIO port to use for the Joybus data line
 * @param pin the GPIO pin to use for the Joybus data line
 * @param rx_timer the TIMER peripheral to use for receiving data
 * @param tx_usart the USART peripheral to use for transmitting data
 * @return 0 on success, negative error code on failure
 */
int joybus_gecko_init(struct joybus_gecko *gecko_bus, GPIO_Port_TypeDef port, uint8_t pin, TIMER_TypeDef *rx_timer,
                      USART_TypeDef *tx_usart);

/** @} */