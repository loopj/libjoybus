/**
 * EFM32/EFR32 Silicon Labs Gecko SDK implementation
 *
 * RX implementation:
 * - Pulse edge timings are captured using a TIMER peripheral directly into memory via DMA
 * - After each series of 16 edges (1 byte), an interrupt is fired
 * - We convert the timings into a byte in the interrupt handler
 * - Ping-pong DMA allows processing of pulse timings while immediately capturing the next byte
 *
 * TX implementation:
 * - Bytes are converted to a line coding suitable for clocking out using a USART peripheral
 * - USART peripheral is in synchronous mode, MSB first, tri-state when idle
 * - Ping-pong DMA allows encoding of next byte while transmitting current byte, no need to pre-encode entire message
 * - After each byte is buffered to the USART, an interrupt is fired
 * - Using the "looped transfer" mode of LDMA, we can move on to the stop bit automatically
 */

#include <stddef.h>

#include "em_cmu.h"
#include "em_gpio.h"
#include "em_ldma.h"
#include "em_timer.h"
#include "em_usart.h"

#include "dmadrv.h"

#include <joybus/bus.h>
#include <joybus/errors.h>
#include <joybus/target.h>
#include <joybus/backend/gecko.h>

#ifdef __ZEPHYR__
#include <zephyr/irq.h>

ISR_DIRECT_DECLARE(ldma_zli_isr)
{
  LDMA_IRQHandler();
  return 0;
}
#endif

// SI bus idle period (in microseconds)
#define BUS_IDLE_US             100

enum {
  BUS_MODE_HOST,
  BUS_MODE_TARGET,
};

enum {
  BUS_STATE_DISABLED,
  BUS_STATE_HOST_IDLE,
  BUS_STATE_HOST_TX,
  BUS_STATE_HOST_RX,
  BUS_STATE_TARGET_TX,
  BUS_STATE_TARGET_RX,
};

// Line coding
static const uint8_t BIT_0       = 0b0001;
static const uint8_t BIT_1       = 0b0111;
static const uint8_t HOST_STOP   = 0b01111111;
static const uint8_t TARGET_STOP = 0b00111111;

static void enter_target_read_mode(struct joybus *bus, bool await_idle);
static void handle_command_response(const uint8_t *buffer, uint8_t length, void *user_data);
static bool ldma_tx_handler(unsigned int chan, unsigned int iteration, void *user_data);

// Get the clock for the given TIMER
static inline CMU_Clock_TypeDef get_timer_clock(TIMER_TypeDef *timer)
{
  switch ((uint32_t)timer) {
#if defined(TIMER0_BASE)
    case TIMER0_BASE:
      return cmuClock_TIMER0;
#endif
#if defined(TIMER1_BASE)
    case TIMER1_BASE:
      return cmuClock_TIMER1;
#endif
#if defined(TIMER2_BASE)
    case TIMER2_BASE:
      return cmuClock_TIMER2;
#endif
#if defined(TIMER3_BASE)
    case TIMER3_BASE:
      return cmuClock_TIMER3;
#endif
#if defined(TIMER4_BASE)
    case TIMER4_BASE:
      return cmuClock_TIMER4;
#endif
    default:
      EFM_ASSERT(0);
      return 0;
  }
}

// Get the clock for the given USART
static inline CMU_Clock_TypeDef get_usart_clock(USART_TypeDef *usart)
{
  switch ((uint32_t)usart) {
#if defined(USART0_BASE)
    case USART0_BASE:
      return cmuClock_USART0;
#endif
#if defined(USART1_BASE)
    case USART1_BASE:
      return cmuClock_USART1;
#endif
    default:
      EFM_ASSERT(0);
      return 0;
  }
}

// Get the LDMA CC0 signal for the given TIMER
static inline unsigned long get_timer_ldma_signal(TIMER_TypeDef *timer)
{
  switch ((uint32_t)timer) {
#if defined(TIMER0_BASE)
    case TIMER0_BASE:
      return ldmaPeripheralSignal_TIMER0_CC0;
#endif
#if defined(TIMER1_BASE)
    case TIMER1_BASE:
      return ldmaPeripheralSignal_TIMER1_CC0;
#endif
#if defined(TIMER2_BASE)
    case TIMER2_BASE:
      return ldmaPeripheralSignal_TIMER2_CC0;
#endif
#if defined(TIMER3_BASE)
    case TIMER3_BASE:
      return ldmaPeripheralSignal_TIMER3_CC0;
#endif
#if defined(TIMER4_BASE)
    case TIMER4_BASE:
      return ldmaPeripheralSignal_TIMER4_CC0;
#endif
    default:
      EFM_ASSERT(0);
      return 0;
  }
}

// Get the LDMA TXBL signal for the given USART
static inline unsigned long get_usart_ldma_signal(USART_TypeDef *usart)
{
  switch ((uint32_t)usart) {
#if defined(USART0_BASE)
    case USART0_BASE:
      return ldmaPeripheralSignal_USART0_TXBL;
#endif
#if defined(USART1_BASE)
    case USART1_BASE:
      return ldmaPeripheralSignal_USART1_TXBL;
#endif
    default:
      EFM_ASSERT(0);
      return 0;
  }
}

// Process received SI edge timings into a byte
static inline void decode_pulses(struct joybus *bus, uint8_t *dest, const uint16_t *src, uint16_t threshold,
                                 uint8_t byte_index)
{
  struct joybus_gecko_data *data = &JOYBUS_GECKO(bus)->data;

  uint8_t result = 0;
  if (byte_index == 0) {
    result |= ((uint16_t)(src[1] - src[0]) < threshold) << 7;
    result |= ((uint16_t)(src[3] - src[2]) < threshold) << 6;
    result |= ((uint16_t)(src[5] - src[4]) < threshold) << 5;
    result |= ((uint16_t)(src[7] - src[6]) < threshold) << 4;
    result |= ((uint16_t)(src[9] - src[8]) < threshold) << 3;
    result |= ((uint16_t)(src[11] - src[10]) < threshold) << 2;
    result |= ((uint16_t)(src[13] - src[12]) < threshold) << 1;
    result |= ((uint16_t)(src[15] - src[14]) < threshold);
    data->rx_trailing_bit = ((uint16_t)(src[17] - src[16]) < threshold);
  } else {
    result |= data->rx_trailing_bit << 7;
    result |= ((uint16_t)(src[1] - src[0]) < threshold) << 6;
    result |= ((uint16_t)(src[3] - src[2]) < threshold) << 5;
    result |= ((uint16_t)(src[5] - src[4]) < threshold) << 4;
    result |= ((uint16_t)(src[7] - src[6]) < threshold) << 3;
    result |= ((uint16_t)(src[9] - src[8]) < threshold) << 2;
    result |= ((uint16_t)(src[11] - src[10]) < threshold) << 1;
    result |= ((uint16_t)(src[13] - src[12]) < threshold);
    data->rx_trailing_bit = ((uint16_t)(src[15] - src[14]) < threshold);
  }

  *dest = result;
}

// Convert a byte to the appropriate line coding for transmission
static inline void encode_byte(uint8_t *dest, uint8_t src)
{
  dest[0] = ((src & 0x80) ? BIT_1 : BIT_0) << 4 | ((src & 0x40) ? BIT_1 : BIT_0);
  dest[1] = ((src & 0x20) ? BIT_1 : BIT_0) << 4 | ((src & 0x10) ? BIT_1 : BIT_0);
  dest[2] = ((src & 0x08) ? BIT_1 : BIT_0) << 4 | ((src & 0x04) ? BIT_1 : BIT_0);
  dest[3] = ((src & 0x02) ? BIT_1 : BIT_0) << 4 | ((src & 0x01) ? BIT_1 : BIT_0);
}

// Adjust TX timings for host/target mode
static void set_tx_timings(struct joybus *bus, uint8_t mode)
{
  struct joybus_gecko_data *data = &JOYBUS_GECKO(bus)->data;

  if (mode == BUS_MODE_TARGET) {
    USART_BaudrateSyncSet(data->tx_usart, 0, data->target_freq * CHIPS_PER_BIT);
    data->tx_descriptors[2].xfer.srcAddr = (uint32_t)&TARGET_STOP;
  } else {
    USART_BaudrateSyncSet(data->tx_usart, 0, data->host_freq * CHIPS_PER_BIT);
    data->tx_descriptors[2].xfer.srcAddr = (uint32_t)&HOST_STOP;
  }
}

// Wait for the Joybus to be idle
static void await_bus_idle(struct joybus *bus)
{
  struct joybus_gecko_data *data = &JOYBUS_GECKO(bus)->data;

  // Start the timer
  TIMER_Enable(data->rx_timer, true);

  while (1) {
    // Wait for the line to go high
    // TODO: Add a timeout
    while (GPIO_PinInGet(data->gpio_port, data->gpio_pin) == 0)
      ;

    // Start timing the bus idle period
    TIMER_CounterSet(data->rx_timer, 0);

    // Wait for either the bus idle period to elapse or line to go low
    // TODO: Add a timeout
    while (GPIO_PinInGet(data->gpio_port, data->gpio_pin) == 1) {
      if (TIMER_CounterGet(data->rx_timer) >= data->bus_idle_period)
        goto idle_detected;
    }
  }

idle_detected:
  // Stop the timer
  TIMER_Enable(data->rx_timer, false);
}

// Handle transfer timeouts
void transfer_timeout(sl_sleeptimer_timer_handle_t *handle, void *user_data)
{
  struct joybus *bus             = (struct joybus *)user_data;
  struct joybus_gecko_data *data = &JOYBUS_GECKO(bus)->data;

  // Timeout occurred, switch back to idle/read mode
  if (bus->target) {
    enter_target_read_mode(bus, true);
  } else {
    data->state = BUS_STATE_HOST_IDLE;
  }

  // Call the transfer complete callback with an error
  if (data->done_callback)
    data->done_callback(bus, -JOYBUS_ERR_TIMEOUT, data->done_user_data);
}

// Handle target rx byte timeouts
void target_rx_timeout(sl_sleeptimer_timer_handle_t *handle, void *user_data)
{
  struct joybus *bus             = (struct joybus *)user_data;
  struct joybus_gecko_data *data = &JOYBUS_GECKO(bus)->data;

  // Timeout occurred, switch back to idle/read mode
  if (bus->target) {
    enter_target_read_mode(bus, true);
  } else {
    data->state = BUS_STATE_HOST_IDLE;
  }
}

static inline uint32_t sl_sleeptimer_us_to_tick(uint32_t time_us)
{
  uint64_t ticks = (uint64_t)time_us * sl_sleeptimer_get_timer_frequency();
  ticks += 1000000 - 1; // ceil: ensure at least the requested delay
  ticks /= 1000000;
  return (uint32_t)ticks;
}

// LDMA interrupt handler for RX, called when a 16 timings have been captured
static bool ldma_rx_handler(unsigned int chan, unsigned int iteration, void *user_data)
{
  struct joybus *bus             = (struct joybus *)user_data;
  struct joybus_gecko_data *data = &JOYBUS_GECKO(bus)->data;

  if (data->state == BUS_STATE_HOST_RX) {
    // Process the received pulses into the byte buffer
    decode_pulses(bus, &data->read_buf[iteration - 1], data->rx_edge_timings[data->rx_current_buffer],
                  data->target_pulse_period_half, iteration - 1);
    data->rx_current_buffer ^= 1;

    if (iteration == data->read_len) {
      // No more bytes expected
      // Stop input capture
      TIMER_Enable(data->rx_timer, false);

      // Switch back to idle/read mode
      if (bus->target) {
        enter_target_read_mode(bus, true);
      } else {
        data->state = BUS_STATE_HOST_IDLE;
      }

      // Call the transfer complete callback
      if (data->done_callback)
        data->done_callback(bus, data->read_len, data->done_user_data);

      // Cancel rx timeout
      sl_sleeptimer_stop_timer(&data->rx_timeout_timer);
    } else {
      // More bytes expected
      // After the first byte, switch to full 16-edge captures
      if (iteration == 1) {
        data->rx_descriptors[0].xfer.xferCnt = EDGES_PER_BYTE - 1;

        sl_sleeptimer_start_timer(&data->rx_timeout_timer, sl_sleeptimer_us_to_tick(60), transfer_timeout, bus, 0,
                                  SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG);
      } else {
        // Reset rx timeout for the next byte
        sl_sleeptimer_restart_timer(&data->rx_timeout_timer, sl_sleeptimer_us_to_tick(60), transfer_timeout, bus, 0,
                                    SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG);
      }
    }
  } else if (data->state == BUS_STATE_TARGET_RX) {
    // Process the received pulses into the byte buffer
    decode_pulses(bus, &data->read_buf[iteration - 1], data->rx_edge_timings[data->rx_current_buffer],
                  data->host_pulse_period_half, iteration - 1);
    data->rx_current_buffer ^= 1;

    // Handle the received byte
    int rc = joybus_target_byte_received(bus->target, data->read_buf, iteration, handle_command_response, bus);
    if (rc == 0) {
      // No more bytes expected
      // Start the response transfer if there is one
      if (data->write_len > 0) {
        // Prepare and start the response transfer
        LDMA->REQDIS_CLR = 1 << data->tx_dma_channel;
        data->state      = BUS_STATE_TARGET_TX;

        // Stop input capture
        TIMER_Enable(data->rx_timer, false);
      } else {
        // Stop input capture
        TIMER_Enable(data->rx_timer, false);

        // No response to send, switch back to read mode or idle
        if (bus->target) {
          enter_target_read_mode(bus, false);
        } else {
          data->state = BUS_STATE_HOST_IDLE;
        }
      }

      // Cancel rx timeout
      sl_sleeptimer_stop_timer(&data->rx_timeout_timer);
    } else if (rc > 0) {
      // More bytes expected
      // After the first byte, switch to 16-edge captures
      if (iteration == 1)
        data->rx_descriptors[0].xfer.xferCnt = EDGES_PER_BYTE - 1;

      // Reset rx timeout for the next byte
      sl_sleeptimer_restart_timer(&data->rx_timeout_timer, sl_sleeptimer_us_to_tick(60), target_rx_timeout, bus, 0,
                                  SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG);

    } else {
      // Error handling command, or command not supported
      // Stop input capture
      TIMER_Enable(data->rx_timer, false);

      // Switch back to idle/read mode
      if (bus->target) {
        enter_target_read_mode(bus, true);
      } else {
        data->state = BUS_STATE_HOST_IDLE;
      }

      // Cancel rx timeout
      sl_sleeptimer_stop_timer(&data->rx_timeout_timer);
    }
  }

  return true;
}

// LDMA interrupt handler for TX, called when each encoded byte is buffered to the USART
static bool ldma_tx_handler(unsigned int chan, unsigned int iteration, void *user_data)
{
  struct joybus *bus             = (struct joybus *)user_data;
  struct joybus_gecko_data *data = &JOYBUS_GECKO(bus)->data;

  // Encode the next byte, if there is one
  if (data->tx_buffered_bytes < data->write_len) {
    encode_byte(data->tx_encoded_bytes[data->tx_current_buffer], data->write_buf[data->tx_buffered_bytes]);
    data->tx_current_buffer ^= 1;
    data->tx_buffered_bytes++;
  }

  if (iteration == data->write_len) {
    // Wait for TX buffer to be empty
    while (!(data->tx_usart->STATUS & USART_STATUS_TXBL))
      ;

    // Wait for low pulse of the stop bit to end
    while (!GPIO_PinInGet(data->gpio_port, data->gpio_pin))
      ;

    if (data->state == BUS_STATE_HOST_TX) {
      // We've finished sending a command (host mode), check if we need to receive a response
      if (data->read_len > 0) {
        // Immediately flip into read mode, we've already pre-armed the RX LDMA
        data->state = BUS_STATE_HOST_RX;
        TIMER_Enable(data->rx_timer, true);

        // Start the RX timeout timer
        sl_sleeptimer_start_timer(&data->rx_timeout_timer, sl_sleeptimer_us_to_tick(100), transfer_timeout, bus, 0,
                                  SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG);
      } else {
        // No reply expected, go idle and call the transfer complete callback
        data->state = BUS_STATE_HOST_IDLE;
        data->done_callback(bus, 0, data->done_user_data);
      }
    } else if (data->state == BUS_STATE_TARGET_TX) {
      // If we are handling a command response (target mode), and a target is
      // still registered, flip back into read mode to listen for the next
      // command. Otherwise, go idle.
      if (bus->target) {
        enter_target_read_mode(bus, false);
      } else {
        data->state = BUS_STATE_HOST_IDLE;
      }
    }
  }

  return true;
}

// Kick off an SI read operation
static void enter_target_read_mode(struct joybus *bus, bool await_idle)
{
  struct joybus_gecko_data *data = &JOYBUS_GECKO(bus)->data;

  data->read_buf   = bus->command_buffer;
  data->read_len   = JOYBUS_BLOCK_SIZE;
  data->read_count = 0;
  data->write_buf  = NULL;
  data->write_len  = 0;

  data->rx_current_buffer              = 0;
  data->rx_descriptors[0].xfer.xferCnt = EDGES_PER_BYTE + 2 - 1;

  // Clear any stale captures
  while (TIMER_CaptureGet(data->rx_timer, 0))
    ;

  // Arm the LDMA transfer
  DMADRV_LdmaStartTransfer(data->rx_dma_channel, &data->rx_config, &data->rx_descriptors[data->rx_current_buffer],
                           ldma_rx_handler, bus);

  // Wait for bus idle
  if (await_idle)
    await_bus_idle(bus);

  // Start the timer to begin capturing
  TIMER_Enable(data->rx_timer, true);

  // Transition state
  data->state = BUS_STATE_TARGET_RX;
}

// Prepare an SI write operation by pre-encoding the first one or two bytes
static inline void prepare_write(struct joybus *bus, const uint8_t *buffer, uint8_t length)
{
  struct joybus_gecko_data *data = &JOYBUS_GECKO(bus)->data;

  // Save the transfer state
  data->write_buf = (uint8_t *)buffer;
  data->write_len = length;

  // With ping-pong looped descriptors, we need to start with the correct descriptor/buffer
  // If the length is odd, we need to start with the second descriptor/buffer
  data->tx_initial_buffer = data->tx_current_buffer = length % 2;

  // Set the loop count for the LDMA transfer
  data->tx_config.ldmaLoopCnt = length - 1;

  // Encode the first byte
  encode_byte(data->tx_encoded_bytes[data->tx_current_buffer], data->write_buf[0]);
  data->tx_current_buffer ^= 1;
  data->tx_buffered_bytes = 1;

  // Encode the next byte, if there is one
  if (length > 1) {
    encode_byte(data->tx_encoded_bytes[data->tx_current_buffer], data->write_buf[1]);
    data->tx_current_buffer ^= 1;
    data->tx_buffered_bytes = 2;
  }

  LDMA->REQDIS_SET = 1 << data->tx_dma_channel;
  DMADRV_LdmaStartTransfer(data->tx_dma_channel, &data->tx_config, &data->tx_descriptors[data->tx_initial_buffer],
                           ldma_tx_handler, bus);
}

// Callback for handling a command response from the registered target
static void handle_command_response(const uint8_t *buffer, uint8_t length, void *user_data)
{
  struct joybus *bus = (struct joybus *)user_data;
  prepare_write(bus, buffer, length);
}

// Enable the RX peripheral and LDMA channel
static int enable_rx(struct joybus *bus)
{
  struct joybus_gecko_data *data = &JOYBUS_GECKO(bus)->data;

  // Enable clocks
  CMU_ClockEnable(get_timer_clock(data->rx_timer), true);

  // Allocate DMA channel
  DMADRV_AllocateChannel(&data->rx_dma_channel, NULL);

  // Initialize timer
  TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;
  timerInit.enable             = false;
  TIMER_Init(data->rx_timer, &timerInit);

  // Configure CC0 for pulse width capture
  TIMER_InitCC_TypeDef timerCCInit = TIMER_INITCC_DEFAULT;
  timerCCInit.edge                 = timerEdgeBoth;
  timerCCInit.mode                 = timerCCModeCapture;
  TIMER_InitCC(data->rx_timer, 0, &timerCCInit);

  // Route timer capture input to the SI GPIO
  GPIO->TIMERROUTE[TIMER_NUM(data->rx_timer)].ROUTEEN = GPIO_TIMER_ROUTEEN_CC0PEN;
  GPIO->TIMERROUTE[TIMER_NUM(data->rx_timer)].CC0ROUTE =
    (data->gpio_port << _GPIO_TIMER_CC0ROUTE_PORT_SHIFT) | (data->gpio_pin << _GPIO_TIMER_CC0ROUTE_PIN_SHIFT);

  // Set up the timings for rx pulses
  uint32_t rx_timer_freq         = CMU_ClockFreqGet(get_timer_clock(data->rx_timer));
  data->host_pulse_period_half   = (rx_timer_freq / data->host_freq) / 2;
  data->target_pulse_period_half = (rx_timer_freq / data->target_freq) / 2;
  data->bus_idle_period          = rx_timer_freq / 1000000UL * BUS_IDLE_US;

  // LDMA RX configuration
  data->rx_config = (LDMA_TransferCfg_t)LDMA_TRANSFER_CFG_PERIPHERAL(get_timer_ldma_signal(data->rx_timer));

  // LDMA RX ping-pong descriptors
  // clang-format off
  data->rx_descriptors[0] = (LDMA_Descriptor_t)
    LDMA_DESCRIPTOR_LINKREL_P2M_WORD(&data->rx_timer->CC[0].ICF, data->rx_edge_timings[0], EDGES_PER_BYTE + 2, 1);
  data->rx_descriptors[0].xfer.size = ldmaCtrlSizeHalf;
  data->rx_descriptors[1] = (LDMA_Descriptor_t)
    LDMA_DESCRIPTOR_LINKREL_P2M_WORD(&data->rx_timer->CC[0].ICF, data->rx_edge_timings[1], EDGES_PER_BYTE, -1);
  data->rx_descriptors[1].xfer.size = ldmaCtrlSizeHalf;
  // clang-format on

  return 0;
}

// Disable the RX peripheral and LDMA channel
static int disable_rx(struct joybus *bus)
{
  struct joybus_gecko_data *data = &JOYBUS_GECKO(bus)->data;

  // Disable timer
  TIMER_Enable(data->rx_timer, false);

  // Remove peripheral route
  GPIO->TIMERROUTE[TIMER_NUM(data->rx_timer)].ROUTEEN = 0;

  // Free DMA channel
  DMADRV_FreeChannel(data->rx_dma_channel);

  return 0;
}

// Enable the TX peripheral and LDMA channel
static int enable_tx(struct joybus *bus)
{
  struct joybus_gecko_data *data = &JOYBUS_GECKO(bus)->data;

  // Enable clocks
  CMU_ClockEnable(get_usart_clock(data->tx_usart), true);

  // Allocate DMA channel
  DMADRV_AllocateChannel(&data->tx_dma_channel, NULL);

  // Initialize USART
  USART_InitSync_TypeDef usartConfig = USART_INITSYNC_DEFAULT;
  usartConfig.baudrate               = data->host_freq * CHIPS_PER_BIT;
  usartConfig.msbf                   = true;
  USART_InitSync(data->tx_usart, &usartConfig);

  // Tri-state the USART TX output
  data->tx_usart->CTRL_SET = USART_CTRL_AUTOTRI;

  // Route USART output to the SI GPIO
  GPIO->USARTROUTE[USART_NUM(data->tx_usart)].ROUTEEN = GPIO_USART_ROUTEEN_TXPEN;
  GPIO->USARTROUTE[USART_NUM(data->tx_usart)].TXROUTE =
    (data->gpio_port << _GPIO_USART_TXROUTE_PORT_SHIFT) | (data->gpio_pin << _GPIO_USART_TXROUTE_PIN_SHIFT);

  // LDMA TX configuration, we'll set the loop for each transfer later
  data->tx_config = (LDMA_TransferCfg_t)LDMA_TRANSFER_CFG_PERIPHERAL_LOOP(get_usart_ldma_signal(data->tx_usart), 1);

  // LDMA TX ping-pong descriptors
  // clang-format off
  data->tx_descriptors[0] = (LDMA_Descriptor_t)
    LDMA_DESCRIPTOR_LINKREL_M2P_BYTE(data->tx_encoded_bytes[0], &(data->tx_usart->TXDATA), CHIPS_PER_BIT, 1);
  data->tx_descriptors[0].xfer.decLoopCnt = 1;
  data->tx_descriptors[1] = (LDMA_Descriptor_t)
    LDMA_DESCRIPTOR_LINKREL_M2P_BYTE(data->tx_encoded_bytes[1], &(data->tx_usart->TXDATA), CHIPS_PER_BIT, -1);
  data->tx_descriptors[1].xfer.decLoopCnt = 1;

  // LDMA stop bit descriptor
  data->tx_descriptors[2] = (LDMA_Descriptor_t)
    LDMA_DESCRIPTOR_SINGLE_M2P_BYTE(&HOST_STOP, &(data->tx_usart->TXDATA), 1);
  // clang-format on

  return 0;
}

// Disable the TX peripheral and LDMA channel
static int disable_tx(struct joybus *bus)
{
  struct joybus_gecko_data *data = &JOYBUS_GECKO(bus)->data;

  // Cancel in-progress TX
  DMADRV_StopTransfer(data->tx_dma_channel);

  // Remove peripheral route
  GPIO->USARTROUTE[USART_NUM(data->tx_usart)].ROUTEEN = 0;

  // Disable USART
  USART_Enable(data->tx_usart, usartDisable);

  // Disable LDMA channel
  DMADRV_FreeChannel(data->tx_dma_channel);

  return 0;
}

static int joybus_gecko_enable(struct joybus *bus)
{
  struct joybus_gecko_data *data = &JOYBUS_GECKO(bus)->data;

  // Initialize LDMA
  DMADRV_Init();

  // Configure the SI GPIO
  CMU_ClockEnable(cmuClock_GPIO, true);
  GPIO_PinModeSet(data->gpio_port, data->gpio_pin, gpioModeWiredAnd, 1);

  // Set LDMA interrupts as highest priority
  NVIC_SetPriority(LDMA_IRQn, 0);

  // Initialize RX and TX peripherals
  enable_rx(bus);
  enable_tx(bus);

  // Initialize sleeptimer for timeouts
  sl_sleeptimer_init();

  // Connect the LDMA interrupt if building for Zephyr
#ifdef __ZEPHYR__
  IRQ_DIRECT_CONNECT(LDMA_IRQn, 0, ldma_zli_isr, IRQ_ZERO_LATENCY);
  irq_enable(LDMA_IRQn);
#endif

  if (bus->target) {
    set_tx_timings(bus, BUS_MODE_TARGET);
    enter_target_read_mode(bus, true);
  } else {
    data->state = BUS_STATE_HOST_IDLE;
  }

  return 0;
}

static int joybus_gecko_disable(struct joybus *bus)
{
  struct joybus_gecko_data *data = &JOYBUS_GECKO(bus)->data;
  if (data->state == BUS_STATE_DISABLED)
    return 0;

  // Disable RX and TX
  disable_rx(bus);
  disable_tx(bus);

  // Reset GPIO pin to input
  GPIO_PinModeSet(data->gpio_port, data->gpio_pin, gpioModeInput, 0);

  data->state = BUS_STATE_DISABLED;

  return 0;
}

static int joybus_gecko_transfer(struct joybus *bus, const uint8_t *write_buf, uint8_t write_len, uint8_t *read_buf,
                                 uint8_t read_len, joybus_transfer_cb_t callback, void *user_data)
{
  struct joybus_gecko_data *data = &JOYBUS_GECKO(bus)->data;

  if (data->state == BUS_STATE_DISABLED)
    return -JOYBUS_ERR_DISABLED;

  if (data->state != BUS_STATE_HOST_IDLE)
    return -JOYBUS_ERR_BUSY;

  // Save the reply context
  data->read_buf       = read_buf;
  data->read_len       = read_len;
  data->read_count     = 0;
  data->done_callback  = callback;
  data->done_user_data = user_data;

  // Mark transfer as started
  data->state = BUS_STATE_HOST_TX;

  // Clear any stale RX captures
  while (TIMER_CaptureGet(data->rx_timer, 0))
    ;

  // Arm the RX DMA channel to receive the response
  DMADRV_LdmaStartTransfer(data->rx_dma_channel, &data->rx_config, &data->rx_descriptors[data->rx_current_buffer],
                           ldma_rx_handler, bus);

  // Kick off the write
  prepare_write(bus, write_buf, write_len);
  LDMA->REQDIS_CLR = 1 << data->tx_dma_channel;

  return 0;
}

static int joybus_gecko_target_register(struct joybus *bus, struct joybus_target *target)
{
  struct joybus_gecko_data *data = &JOYBUS_GECKO(bus)->data;

  // Save the target
  bus->target = target;

  // Immediately start listening for commands if the bus is enabled
  if (data->state != BUS_STATE_DISABLED) {
    set_tx_timings(bus, BUS_MODE_TARGET);
    enter_target_read_mode(bus, true);
  }

  return 0;
}

static int joybus_gecko_target_unregister(struct joybus *bus, struct joybus_target *target)
{
  struct joybus_gecko_data *data = &JOYBUS_GECKO(bus)->data;

  if (data->state != BUS_STATE_DISABLED) {
    // Cancel any ongoing RX/TX
    TIMER_Enable(data->rx_timer, false);
    DMADRV_StopTransfer(data->tx_dma_channel);

    // Set TX timings back to host mode
    set_tx_timings(bus, BUS_MODE_HOST);
  }

  // Set bus state to idle
  data->state = BUS_STATE_HOST_IDLE;

  // Clear the target
  bus->target = NULL;

  return 0;
}

static const struct joybus_api gecko_api = {
  .enable            = joybus_gecko_enable,
  .disable           = joybus_gecko_disable,
  .transfer          = joybus_gecko_transfer,
  .target_register   = joybus_gecko_target_register,
  .target_unregister = joybus_gecko_target_unregister,
};

int joybus_gecko_init(struct joybus_gecko *gecko_bus, GPIO_Port_TypeDef port, uint8_t pin, TIMER_TypeDef *rx_timer,
                      USART_TypeDef *tx_usart)
{
  struct joybus *bus = JOYBUS(gecko_bus);
  bus->api           = &gecko_api;
  bus->target        = NULL;

  // Save the joybus configuration
  struct joybus_gecko_data *data = &gecko_bus->data;
  data->gpio_port                = port;
  data->gpio_pin                 = pin;
  data->rx_timer                 = rx_timer;
  data->tx_usart                 = tx_usart;
  data->state                    = BUS_STATE_DISABLED;
  data->host_freq                = JOYBUS_FREQ_CONSOLE;
  data->target_freq              = JOYBUS_FREQ_GCC;

  return 0;
}