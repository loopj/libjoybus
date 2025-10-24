#include <stdio.h>

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include <joybus/joybus.h>
#include <joybus/backend/rp2xxx.h>

#define SI_DATA_GPIO  12
#define LED_GPIO      13

struct joybus_rp2xxx rp2xxx_bus;
struct joybus *bus = JOYBUS(&rp2xxx_bus);

uint8_t response[JOYBUS_BLOCK_SIZE] = {0};

static uint16_t last_buttons = 0;
static uint16_t down_buttons = 0;
static uint16_t up_buttons   = 0;

void identify_cb(struct joybus *bus, int result, void *user_data)
{
}

void read_origin_cb(struct joybus *bus, int result, void *user_data)
{
}

void read_cb(struct joybus *bus, int result, void *user_data)
{
  uint16_t buttons = ((response[1] << 8) | response[0]) & JOYBUS_GCN_BUTTON_MASK;

  down_buttons = (buttons ^ last_buttons) & buttons;
  up_buttons   = (buttons ^ last_buttons) & last_buttons;

  last_buttons = buttons;
}

int main()
{
  // Initialize stdio and wait a bit for USB to be ready
  stdio_init_all();

  // Set up GPIO for status LED
  gpio_init(LED_GPIO);
  gpio_set_dir(LED_GPIO, GPIO_OUT);
  gpio_put(LED_GPIO, 0);

  // Initialize and enable Joybus
  joybus_rp2xxx_init(&rp2xxx_bus, SI_DATA_GPIO, pio0);
  joybus_enable(bus);

  // Ask any connected targets for their ID
  joybus_identify(bus, response, identify_cb, NULL);
  sleep_ms(100);

  // Request controller origin
  joybus_gcn_read_origin(bus, response, read_origin_cb, NULL);
  sleep_ms(100);

  // TODO: We should check for a response, and check if the response is valid

  while (true) {
    // Read the controller state
    joybus_gcn_read(bus, JOYBUS_GCN_ANALOG_MODE_3, JOYBUS_GCN_MOTOR_STOP, response, read_cb, NULL);

    // Light the LED while the A button is held
    gpio_put(LED_GPIO, last_buttons & JOYBUS_GCN_BUTTON_A);

    // Wait a bit before reading again
    sleep_ms(16);
  }

  return 0;
}
