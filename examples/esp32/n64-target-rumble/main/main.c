#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>

#include <joybus/joybus.h>
#include <joybus/backend/esp32.h>

// GPIOs for the Joybus data line and the rumble motor
#define JOYBUS_GPIO 1
#define MOTOR_GPIO  2

// Joybus and target instances.
static struct joybus_esp32 esp32_bus;
static struct joybus *bus = JOYBUS(&esp32_bus);
static struct joybus_target_n64_controller n64_controller;
static struct joybus_target_n64_pak_rumble rumble_pak;

// Drive the motor as the console asks, from interrupt context
JOYBUS_RAM_FUNC
static void on_motor_change(struct joybus_target_n64_pak_rumble *pak, bool active)
{
  gpio_set_level(MOTOR_GPIO, active);
}

void app_main(void)
{
  // Initialize the Joybus.
  joybus_esp32_init(&esp32_bus, joybus_esp32_config_default(JOYBUS_GPIO));

  // Initialize a N64 controller target as a standard controller.
  joybus_target_n64_controller_init(&n64_controller);

  // Initialize a rumble pak and plug it into the controller.
  joybus_target_n64_pak_rumble_init(&rumble_pak);
  joybus_target_n64_pak_rumble_set_motor_cb(&rumble_pak, on_motor_change);
  joybus_target_n64_controller_attach_pak(&n64_controller, JOYBUS_TARGET_N64_PAK(&rumble_pak));

  // Attach the target to the bus.
  joybus_attach_target(bus, JOYBUS_TARGET(&n64_controller));

  // Enable the Joybus in target mode.
  joybus_enable(bus, JOYBUS_MODE_TARGET);

  // Configure the motor GPIO as an output
  gpio_config_t motor_config = {.pin_bit_mask = 1ULL << MOTOR_GPIO, .mode = GPIO_MODE_OUTPUT};
  gpio_config(&motor_config);
  gpio_set_level(MOTOR_GPIO, 0);

  while (1) {
    // Read your inputs into n64_controller.input here

    // Chill for a bit
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
