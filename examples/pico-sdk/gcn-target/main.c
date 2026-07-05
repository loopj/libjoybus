#include <stdio.h>

#include "pico/stdlib.h"

#include <joybus/joybus.h>
#include <joybus/backend/rp2xxx.h>

#define JOYBUS_GPIO       0
#define BUTTON_A_GPIO     1

struct joybus_rp2xxx rp2xxx_bus;
struct joybus *bus = JOYBUS(&rp2xxx_bus);

static struct joybus_target_gcn_controller gcn_controller;

int main()
{
  // Configure GPIO as input with pull-up (active low)
  gpio_init(BUTTON_A_GPIO);
  gpio_pull_up(BUTTON_A_GPIO);

  // Initialize the Joybus
  joybus_rp2xxx_init(&rp2xxx_bus, JOYBUS_GPIO, pio0);

  // Initialize a GameCube controller target as a standard controller
  joybus_target_gcn_controller_init(&gcn_controller);

  // Attach the target to the bus
  joybus_attach_target(bus, JOYBUS_TARGET(&gcn_controller));

  // Enable the Joybus in target mode
  joybus_enable(bus, JOYBUS_MODE_TARGET);

  while (1) {
    // Clear previous button state
    gcn_controller.input.buttons &= ~JOYBUS_GCN_BUTTON_MASK;

    // Simulate pressing the A button when the BUTTON_A_GPIO (active low) is pressed
    if (gpio_get(BUTTON_A_GPIO) == 0)
      gcn_controller.input.buttons |= JOYBUS_GCN_BUTTON_A;

    // Chill for a bit
    sleep_ms(10);
  }

  return 0;
}
