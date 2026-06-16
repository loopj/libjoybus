#include <stdio.h>

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include <joybus/joybus.h>
#include <joybus/backend/rp2xxx.h>

#define JOYBUS_GPIO             12
#define LED_GPIO                13

#define JOYBUS_POLL_INTERVAL_MS 15

// Joybus instance
struct joybus_rp2xxx rp2xxx_bus;
struct joybus *bus = JOYBUS(&rp2xxx_bus);

// Buffers for Joybus responses
struct joybus_id joybus_id;
struct joybus_gcn_controller_state controller_state;
uint8_t joybus_response[JOYBUS_BLOCK_SIZE] = {0};

// Polling mode
enum { POLL_MODE_IDENTIFY, POLL_MODE_READ };
static uint8_t poll_mode = POLL_MODE_IDENTIFY;

void joybus_identify_cb(struct joybus *bus, int result, void *user_data)
{
  // Stay in identify mode on any Joybus error
  if (result < 0)
    return;

  // Check it's a GameCube controller
  uint16_t type = joybus_id.type;
  if (!(type & JOYBUS_TYPE_GCN_DEVICE))
    return;

  // Check we've received data if it's a wireless controller
  if ((type & JOYBUS_TYPE_GCN_WIRELESS) && !(type & JOYBUS_TYPE_GCN_WIRELESS_RECEIVED))
    return;

  // Check it's a standard controller
  if (!(type & JOYBUS_TYPE_GCN_STANDARD))
    return;

  // Move to polling for input
  poll_mode = POLL_MODE_READ;
}

void joybus_read_cb(struct joybus *bus, int result, void *user_data)
{
  // Switch back to identify mode on any Joybus error
  if (result < 0) {
    poll_mode = POLL_MODE_IDENTIFY;
    return;
  }

  // Light the LED while the A button is held
  gpio_put(LED_GPIO, controller_state.buttons & JOYBUS_GCN_BUTTON_A);
}

// Poll the Joybus at regular intervals
static bool poll_task(struct repeating_timer *timer)
{
  // Poll Joybus for data
  switch (poll_mode) {
    case POLL_MODE_IDENTIFY:
      gpio_put(LED_GPIO, 0);
      joybus_identify_async(bus, &joybus_id, joybus_identify_cb, NULL);
      break;

    case POLL_MODE_READ:
      joybus_gcn_read_async(bus, JOYBUS_GCN_ANALOG_MODE_3, JOYBUS_GCN_MOTOR_STOP, &controller_state, joybus_read_cb, NULL);
      break;
  }

  return true;
}

int main()
{
  // Initialize stdio and wait a bit for USB to be ready
  stdio_init_all();

  // Set up GPIO for status LED
  gpio_init(LED_GPIO);
  gpio_set_dir(LED_GPIO, GPIO_OUT);
  gpio_put(LED_GPIO, 0);

  // Initialize Joybus
  joybus_rp2xxx_init(&rp2xxx_bus, JOYBUS_GPIO, pio0);
  joybus_enable(bus);

  // Poll for Joybus data at regular intervals
  struct repeating_timer poll_timer;
  add_repeating_timer_ms(JOYBUS_POLL_INTERVAL_MS, poll_task, NULL, &poll_timer);

  // No work to do in main loop
  while (true)
    ;

  return 0;
}
