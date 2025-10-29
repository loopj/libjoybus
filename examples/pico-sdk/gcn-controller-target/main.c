#include <stdio.h>

#include "pico/stdlib.h"

#include <joybus/joybus.h>
#include <joybus/backend/rp2xxx.h>

#define BUTTON_GPIO   11
#define SI_DATA_GPIO  12

struct joybus_rp2xxx rp2xxx_bus;
struct joybus *bus = JOYBUS(&rp2xxx_bus);

static struct joybus_gc_controller gc_controller;

int main()
{
  // Initialize the Joybus
  joybus_rp2xxx_init(&rp2xxx_bus, SI_DATA_GPIO, pio0);
  joybus_enable(bus);

  // Initialize a GameCube controller target as a standard controller
  joybus_gc_controller_init(&gc_controller, JOYBUS_GAMECUBE_CONTROLLER);

  // Register the target on the bus
  joybus_target_register(bus, JOYBUS_TARGET(&gc_controller));

  while (1) {
    // Clear previous button state
    gc_controller.input.buttons &= ~JOYBUS_GCN_BUTTON_MASK;

    // Simulate pressing the A button when the BUTTON_GPIO (active low) is pressed
    if (gpio_get(BUTTON_GPIO) == 0)
      gc_controller.input.buttons |= JOYBUS_GCN_BUTTON_A;

    // Chill for a bit
    sleep_ms(10);
  }

  return 0;
}
