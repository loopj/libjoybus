#include <stdio.h>

#include "pico/stdlib.h"

#include <joybus/joybus.h>
#include <joybus/backend/rp2xxx.h>

#define JOYBUS_GPIO       0
#define BUTTON_A_GPIO     1

struct joybus_rp2xxx rp2xxx_bus;
struct joybus *bus = JOYBUS(&rp2xxx_bus);

static struct joybus_n64_controller n64_controller;

int main()
{
  // Configure GPIO as input with pull-up (active low)
  gpio_init(BUTTON_A_GPIO);
  gpio_pull_up(BUTTON_A_GPIO);

  // Initialize the Joybus
  joybus_rp2xxx_init(&rp2xxx_bus, JOYBUS_GPIO, pio0);
  joybus_enable(bus);

  // Initialize a N64 controller target as a standard controller
  joybus_n64_controller_init(&n64_controller);

  // Register the target on the bus
  joybus_target_register(bus, JOYBUS_TARGET(&n64_controller));

  while (1) {
    // Clear previous button state
    n64_controller.input.buttons &= ~JOYBUS_N64_BUTTON_MASK;

    // Simulate button presses based on GPIO input (active low)
    if (gpio_get(BUTTON_A_GPIO) == 0)
      n64_controller.input.buttons |= JOYBUS_N64_BUTTON_A;

    // Chill for a bit
    sleep_ms(10);
  }

  return 0;
}
