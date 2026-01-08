#include "em_cmu.h"
#include "em_gpio.h"

#include "sl_main_init.h"
#include "sl_udelay.h"

#include <joybus/joybus.h>
#include <joybus/backend/gecko.h>

// Change these defines to match your hardware setup
#define JOYBUS_DATA_PORT  gpioPortD
#define JOYBUS_DATA_PIN   3
#define JOYBUS_TIMER      TIMER0
#define JOYBUS_USART      USART0
#define BTN_PORT          gpioPortC
#define BTN_PIN           7

// Joybus instance
struct joybus_gecko gecko_bus;
struct joybus *bus = JOYBUS(&gecko_bus);

// GameCube controller target instance
static struct joybus_gc_controller gc_controller;

int main()
{
  // Initialize chip
  sl_main_init();

  // Initialize GPIO for button
  CMU_ClockEnable(cmuClock_GPIO, true);
  GPIO_PinModeSet(BTN_PORT, BTN_PIN, gpioModeInput, 1);

  // Initialize the Joybus
  joybus_gecko_init(&gecko_bus, JOYBUS_DATA_PORT, JOYBUS_DATA_PIN, JOYBUS_TIMER, JOYBUS_USART);
  joybus_enable(bus);

  // Initialize a GameCube controller target as a standard controller
  joybus_gc_controller_init(&gc_controller, JOYBUS_GAMECUBE_CONTROLLER);

  // Register the target on the bus
  joybus_target_register(bus, JOYBUS_TARGET(&gc_controller));

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