#include <stdio.h>

#include "hardware/adc.h"

#include <joybus/joybus.h>
#include <joybus/backend/rp2xxx.h>

// GPIO definitions for buttons and stick axes
#define JOYBUS_GPIO         0
#define BUTTON_A_GPIO       1
#define BUTTON_B_GPIO       2
#define BUTTON_Z_GPIO       3
#define BUTTON_START_GPIO   4
#define BUTTON_UP_GPIO      5
#define BUTTON_DOWN_GPIO    6
#define BUTTON_LEFT_GPIO    7
#define BUTTON_RIGHT_GPIO   8
#define BUTTON_L_GPIO       9
#define BUTTON_R_GPIO       10
#define BUTTON_C_UP_GPIO    11
#define BUTTON_C_DOWN_GPIO  12
#define BUTTON_C_LEFT_GPIO  13
#define BUTTON_C_RIGHT_GPIO 14
#define X_AXIS_GPIO         26
#define Y_AXIS_GPIO         27

// Macro to convert GPIO number to ADC channel
#define ADC_CHANNEL(gpio)   ((gpio) - ADC_BASE_PIN)

// Joybus and target instances
static struct joybus_rp2xxx rp2xxx_bus;
static struct joybus *bus = JOYBUS(&rp2xxx_bus);
static struct joybus_target_n64_controller n64_controller;

// Mapping of button GPIOs to their corresponding bits in the N64 controller input state
const uint button_map[][2] = {
  {BUTTON_A_GPIO, JOYBUS_N64_BUTTON_A},           {BUTTON_B_GPIO, JOYBUS_N64_BUTTON_B},
  {BUTTON_Z_GPIO, JOYBUS_N64_BUTTON_Z},           {BUTTON_START_GPIO, JOYBUS_N64_BUTTON_START},
  {BUTTON_UP_GPIO, JOYBUS_N64_BUTTON_UP},         {BUTTON_DOWN_GPIO, JOYBUS_N64_BUTTON_DOWN},
  {BUTTON_LEFT_GPIO, JOYBUS_N64_BUTTON_LEFT},     {BUTTON_RIGHT_GPIO, JOYBUS_N64_BUTTON_RIGHT},
  {BUTTON_L_GPIO, JOYBUS_N64_BUTTON_L},           {BUTTON_R_GPIO, JOYBUS_N64_BUTTON_R},
  {BUTTON_C_UP_GPIO, JOYBUS_N64_BUTTON_C_UP},     {BUTTON_C_DOWN_GPIO, JOYBUS_N64_BUTTON_C_DOWN},
  {BUTTON_C_LEFT_GPIO, JOYBUS_N64_BUTTON_C_LEFT}, {BUTTON_C_RIGHT_GPIO, JOYBUS_N64_BUTTON_C_RIGHT},
};

// Helper function to read stick axis value from ADC and convert to signed 8-bit range
static int8_t read_stick_axis(uint gpio, int origin, bool inverted)
{
  adc_select_input(ADC_CHANNEL(gpio));
  int v = (int)adc_read() - origin;
  return (int8_t)((inverted ? -v : v) >> 4);
}

int main()
{
  // Initialize the Joybus in target mode
  joybus_rp2xxx_init(&rp2xxx_bus, JOYBUS_MODE_TARGET, JOYBUS_GPIO, pio0);

  // Initialize a N64 controller target as a standard controller
  joybus_target_n64_controller_init(&n64_controller);

  // Register the target on the bus
  joybus_target_register(bus, JOYBUS_TARGET(&n64_controller));

  // Enable the Joybus
  joybus_enable(bus);

  // Configure button GPIOs as input with pull-up (active low)
  for (size_t i = 0; i < sizeof(button_map) / sizeof(button_map[0]); i++) {
    gpio_init(button_map[i][0]);
    gpio_pull_up(button_map[i][0]);
  }

  // Initialize ADC for reading stick positions
  adc_init();
  adc_gpio_init(X_AXIS_GPIO);
  adc_gpio_init(Y_AXIS_GPIO);

  while (1) {
    // Clear previous button state
    n64_controller.input.buttons &= ~JOYBUS_N64_BUTTON_MASK;

    // Read button state from each GPIO and set corresponding bits in the controller input
    for (size_t i = 0; i < sizeof(button_map) / sizeof(button_map[0]); i++) {
      if (gpio_get(button_map[i][0]) == 0) {
        n64_controller.input.buttons |= button_map[i][1];
      }
    }

    // Read stick positions from ADC and update controller input
    n64_controller.input.stick_x = read_stick_axis(X_AXIS_GPIO, 2048, false);
    n64_controller.input.stick_y = read_stick_axis(Y_AXIS_GPIO, 2048, true);

    // Chill for a bit
    sleep_ms(10);
  }

  return 0;
}
