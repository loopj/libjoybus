#include "em_chip.h"
#include "em_cmu.h"
#include "em_gpio.h"

#include "sl_sleeptimer.h"
#include "sl_udelay.h"

#include <joybus/joybus.h>
#include <joybus/backend/gecko.h>

#define JOYBUS_PORT             gpioPortD
#define JOYBUS_PIN              3
#define JOYBUS_TIMER            TIMER0
#define JOYBUS_USART            USART0

#define LED_PORT                gpioPortA
#define LED_PIN                 4

#define JOYBUS_POLL_INTERVAL_MS 15

// Joybus instance
struct joybus_gecko gecko_bus;
struct joybus *bus = JOYBUS(&gecko_bus);

// Buffer for Joybus responses
static uint8_t joybus_response[JOYBUS_BLOCK_SIZE] = {0};

// Polling mode
enum { POLL_MODE_IDENTIFY, POLL_MODE_READ };
static uint8_t poll_mode = POLL_MODE_IDENTIFY;

void joybus_identify_cb(struct joybus *bus, int result, void *user_data)
{
  // Stay in identify mode on any Joybus error
  if (result < 0)
    return;

  // Check it's a GameCube controller
  uint16_t type = joybus_id_get_type(joybus_response);
  if (!(type & JOYBUS_ID_GCN_DEVICE))
    return;

  // Check we've received data if it's a wireless controller
  if ((type & JOYBUS_ID_GCN_WIRELESS) && !(type & JOYBUS_ID_GCN_WIRELESS_RECEIVED))
    return;

  // Check it's a standard controller
  if (!(type & JOYBUS_ID_GCN_STANDARD))
    return;

  // Move to polling for input
  poll_mode = POLL_MODE_READ;
}

void joybus_read_cb(struct joybus *bus, int result, void *user_data)
{
  // Switch back to identify mode on any Joybus error
  if (result < 0) {
    poll_mode = POLL_MODE_IDENTIFY;
    return;
  }

  // Extract button states
  uint16_t buttons = ((joybus_response[1] << 8) | joybus_response[0]) & JOYBUS_GCN_BUTTON_MASK;

  // Light the LED while the A button is held
  if (buttons & JOYBUS_GCN_BUTTON_A) {
    GPIO_PinOutSet(LED_PORT, LED_PIN);
  } else {
    GPIO_PinOutClear(LED_PORT, LED_PIN);
  }
}

// Initialize system clocks
static void clock_init()
{
  // HFXO initialization
  CMU_HFXOInit_TypeDef hfxoInit = CMU_HFXOINIT_DEFAULT;
  hfxoInit.ctuneXoAna           = 121;
  hfxoInit.ctuneXiAna           = 121;
  CMU_HFXOInit(&hfxoInit);
  SystemHFXOClockSet(38400000);

  // PLL initialization
  CMU_DPLLInit_TypeDef dpllInit = CMU_DPLL_HFXO_TO_76_8MHZ;
  bool dpllLock                 = false;
  while (!dpllLock)
    dpllLock = CMU_DPLLLock(&dpllInit);

  CMU_ClockSelectSet(cmuClock_SYSCLK, cmuSelect_HFRCODPLL);
  CMU_ClockSelectSet(cmuClock_EM01GRPACLK, cmuSelect_HFXO);
}

// Poll Joybus for data
static void poll_task(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  switch (poll_mode) {
    case POLL_MODE_IDENTIFY:
      joybus_identify(bus, joybus_response, joybus_identify_cb, NULL);
      break;

    case POLL_MODE_READ:
      joybus_gcn_read(bus, JOYBUS_GCN_ANALOG_MODE_3, JOYBUS_GCN_MOTOR_STOP, joybus_response, joybus_read_cb, NULL);
      break;
  }
}

int main()
{
  // Initialize chip and clocks
  CHIP_Init();
  clock_init();

  // Set up GPIO for status LED
  CMU_ClockEnable(cmuClock_GPIO, true);
  GPIO_PinModeSet(LED_PORT, LED_PIN, gpioModePushPull, 0);

  // Initialize Joybus
  joybus_gecko_init(&gecko_bus, JOYBUS_PORT, JOYBUS_PIN, JOYBUS_TIMER, JOYBUS_USART);
  joybus_enable(bus);

  // Poll for Joybus data at regular intervals
  sl_sleeptimer_timer_handle_t poll_timer;
  sl_sleeptimer_init();
  sl_sleeptimer_start_periodic_timer(&poll_timer, sl_sleeptimer_ms_to_tick(JOYBUS_POLL_INTERVAL_MS), poll_task, bus, 0,
                                     SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG);

  // No work to do in main loop
  while (true)
    ;

  return 0;
}