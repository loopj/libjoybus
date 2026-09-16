#include "pico/stdlib.h"

#include <joybus/joybus.h>
#include <joybus/backend/rp2xxx.h>

// GPIOs for the Joybus data line and the rumble motor
#define JOYBUS_GPIO 0
#define MOTOR_GPIO  15

// Joybus and target instances
static struct joybus_rp2xxx rp2xxx_bus;
static struct joybus *bus = JOYBUS(&rp2xxx_bus);
static struct joybus_target_n64_controller n64_controller;
static struct joybus_target_n64_pak_rumble rumble_pak;

// Drive the motor as the console asks, from interrupt context
JOYBUS_RAM_FUNC
static void on_motor_change(struct joybus_target_n64_pak_rumble *pak, bool active)
{
  gpio_put(MOTOR_GPIO, active);
}

int main()
{
  // Initialize the Joybus
  joybus_rp2xxx_init(&rp2xxx_bus, joybus_rp2xxx_config_default(JOYBUS_GPIO));

  // Initialize a N64 controller target as a standard controller
  joybus_target_n64_controller_init(&n64_controller);

  // Initialize a rumble pak and plug it into the controller
  joybus_target_n64_pak_rumble_init(&rumble_pak);
  joybus_target_n64_pak_rumble_set_motor_cb(&rumble_pak, on_motor_change);
  joybus_target_n64_controller_attach_pak(&n64_controller, JOYBUS_TARGET_N64_PAK(&rumble_pak));

  // Attach the target to the bus
  joybus_attach_target(bus, JOYBUS_TARGET(&n64_controller));

  // Enable the Joybus in target mode
  joybus_enable(bus, JOYBUS_MODE_TARGET);

  // Configure the motor GPIO as an output
  gpio_init(MOTOR_GPIO);
  gpio_set_dir(MOTOR_GPIO, GPIO_OUT);
  gpio_put(MOTOR_GPIO, 0);

  while (1) {
    // Read your inputs into n64_controller.input here

    // Chill for a bit
    sleep_ms(10);
  }

  return 0;
}
