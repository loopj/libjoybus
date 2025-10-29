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

#define BTN_PORT          gpioPortC
#define BTN_PIN           7

struct joybus_gecko gecko_bus;
struct joybus *bus = JOYBUS(&gecko_bus);

static struct joybus_gc_controller gc_controller;

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

  // Set up GPIO for button
  CMU_ClockEnable(cmuClock_GPIO, true);
  GPIO_PinModeSet(BTN_PORT, BTN_PIN, gpioModeInput, 1);

  // Initialize the Joybus
  joybus_gecko_init(&gecko_bus, JOYBUS_DATA_PORT, JOYBUS_DATA_PIN, JOYBUS_TIMER, JOYBUS_USART);
  joybus_enable(bus);

  // Initialize a GameCube controller target as a standard controller
  joybus_gc_controller_init(&gc_controller, JOYBUS_GAMECUBE_CONTROLLER);

  // Register the target on the bus
  joybus_target_register(bus, JOYBUS_TARGET(&gc_controller));

  // Wait a bit before starting
  sl_udelay_wait(100000);

  while (1) {
    // Clear previous button state
    gc_controller.input.buttons &= ~JOYBUS_GCN_BUTTON_MASK;

    // Simulate pressing the A button when the BUTTON_GPIO (active low) is pressed
    if (GPIO_PinInGet(BTN_PORT, BTN_PIN) == 0)
      gc_controller.input.buttons |= JOYBUS_GCN_BUTTON_A;

    // Chill for a bit
    sl_udelay_wait(10000);
  }
}