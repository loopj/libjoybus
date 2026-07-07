#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>

#include <joybus/joybus.h>
#include <joybus/backend/esp32.h>

// GPIO for the Joybus data line
#define JOYBUS_GPIO         1
#define BUTTON_A_GPIO       2

// Joybus and target instances.
static struct joybus_esp32 esp32_bus;
static struct joybus *bus = JOYBUS(&esp32_bus);
static struct joybus_target_n64_controller n64_controller;

// Mapping of button GPIOs to their corresponding bits in the N64 controller input state
const unsigned int button_map[][2] = {
  {BUTTON_A_GPIO, JOYBUS_N64_BUTTON_A},
};

void app_main(void)
{
  // Initialize the Joybus.
  joybus_esp32_init(&esp32_bus, joybus_esp32_config_default(JOYBUS_GPIO));

  // Initialize a N64 controller target as a standard controller.
  joybus_target_n64_controller_init(&n64_controller);

  // Attach the target to the bus.
  joybus_attach_target(bus, JOYBUS_TARGET(&n64_controller));

  // Enable the Joybus in target mode.
  joybus_enable(bus, JOYBUS_MODE_TARGET);

  // Configure all button GPIOs as inputs with pull-ups (active low)
  uint64_t button_pins = 0;
  for (int i = 0; i < sizeof(button_map) / sizeof(button_map[0]); i++)
    button_pins |= 1ULL << button_map[i][0];

  gpio_config_t button_config = {
    .pin_bit_mask = button_pins,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = true,
  };

  gpio_config(&button_config);

  while (1) {
    // Clear previous button state
    n64_controller.input.buttons &= ~JOYBUS_N64_BUTTON_MASK;

    // Read button state from each GPIO and set corresponding bits in the controller input
    for (int i = 0; i < sizeof(button_map) / sizeof(button_map[0]); i++) {
      if (gpio_get_level(button_map[i][0]) == 0) {
        n64_controller.input.buttons |= button_map[i][1];
      }
    }

    // Chill for a bit
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
