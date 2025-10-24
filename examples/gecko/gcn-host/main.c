#include "em_chip.h"
#include "em_cmu.h"
#include "em_gpio.h"

#include "sl_udelay.h"

#include <joybus/joybus.h>
#include <joybus/backend/gecko.h>

#define JOYBUS_DATA_PORT  gpioPortD
#define JOYBUS_DATA_PIN   3
#define JOYBUS_TIMER      TIMER0
#define JOYBUS_USART      USART0

#define LED_PORT      gpioPortA
#define LED_PIN       4

struct joybus_gecko gecko_bus;
struct joybus *bus = JOYBUS(&gecko_bus);

static uint8_t response[JOYBUS_BLOCK_SIZE] = {0};

static uint16_t last_buttons = 0;
static uint16_t down_buttons = 0;
static uint16_t up_buttons   = 0;

static void identify_cb(struct joybus *bus, int result, void *user_data)
{
}

static void read_cb(struct joybus *bus, int result, void *user_data)
{
  uint16_t buttons = ((response[1] << 8) | response[0]) & JOYBUS_GCN_BUTTON_MASK;

  down_buttons = (buttons ^ last_buttons) & buttons;
  up_buttons   = (buttons ^ last_buttons) & last_buttons;

  last_buttons = buttons;
}

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

int main()
{
  // Initialize chip and clocks
  CHIP_Init();
  clock_init();

  // Set up GPIO for status LED
  CMU_ClockEnable(cmuClock_GPIO, true);
  GPIO_PinModeSet(LED_PORT, LED_PIN, gpioModePushPull, 0);

  // Initialize and enable the Joybus
  joybus_gecko_init(&gecko_bus, JOYBUS_DATA_PORT, JOYBUS_DATA_PIN, JOYBUS_TIMER, JOYBUS_USART);
  joybus_enable(bus);
  sl_udelay_wait(100000);

  // Ask any connected target for their ID
  joybus_identify(bus, response, identify_cb, NULL);
  sl_udelay_wait(100000);

  while (1) {
    // Read the controller state
    joybus_gcn_read(bus, JOYBUS_GCN_ANALOG_MODE_3, JOYBUS_GCN_MOTOR_STOP, response, read_cb, NULL);

    // Toggle LED on PA4 based on A button state
    if (last_buttons & JOYBUS_GCN_BUTTON_A) {
      GPIO_PinOutSet(LED_PORT, LED_PIN);
    } else {
      GPIO_PinOutClear(LED_PORT, LED_PIN);
    }

    // Wait a bit before reading again
    sl_udelay_wait(10000);
  }
}