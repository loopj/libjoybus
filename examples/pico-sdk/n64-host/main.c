#include <stdio.h>

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include <joybus/joybus.h>
#include "joybus/host/n64_rumble_pak.h"
#include <joybus/backend/rp2xxx.h>

#define JOYBUS_GPIO           12
#define LED_GPIO              13

#define POLL_INTERVAL_MS      16
#define IDENTIFY_INTERVAL_MS  2000

// Joybus instance
struct joybus_rp2xxx rp2xxx_bus;
struct joybus *bus = JOYBUS(&rp2xxx_bus);

// Controller and accessory state
static struct joybus_id id;
static struct joybus_n64_controller_state controller_state;
static bool controller_present = false;
static bool rumble_available   = false;
static bool rumble_on          = false;

// Poll the controller for presence, input state, and accessory changes
static void poll_controller(void)
{
  // Re-identify at startup, and periodically to track accessory changes
  static absolute_time_t next_identify;
  if (!controller_present || time_reached(next_identify)) {
    next_identify = make_timeout_time_ms(IDENTIFY_INTERVAL_MS);

    if (joybus_identify(bus, &id) >= 0) {
      controller_present = id.type & JOYBUS_DEVICE_N64_CONTROLLER;

      bool pak = controller_present && (id.status & JOYBUS_STATUS_N64_PAK_PRESENT);
      if (pak && (!rumble_available || joybus_id_n64_pak_changed(&id))) {
        // Initialize on first sight, and again whenever the pak was swapped
        rumble_available = joybus_n64_rumble_pak_init(bus) >= 0;
        rumble_on        = false;
      } else if (!pak) {
        rumble_available = false;
      }
    } else {
      controller_present = false;
      rumble_available   = false;
    }
  }

  // Read the latest input state
  if (controller_present)
    joybus_n64_read(bus, &controller_state);
}

// Start/stop the rumble motor when the requested state changes
static void set_rumble(bool on)
{
  // Ignore if rumble pak not inserted, or state hasn't changed
  if (!rumble_available || on == rumble_on)
    return;

  // Set rumble state
  if (on) {
    rumble_on = joybus_n64_rumble_pak_start(bus) >= 0;
  } else if (joybus_n64_rumble_pak_stop(bus) >= 0) {
    rumble_on = false;
  }
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
  joybus_rp2xxx_init(&rp2xxx_bus, JOYBUS_MODE_HOST, JOYBUS_GPIO, pio0);
  joybus_enable(bus);

  // Poll the controller once per "frame"
  absolute_time_t next_frame = get_absolute_time();
  while (true) {
    poll_controller();

    // Light the LED while A is held
    if (controller_state.buttons & JOYBUS_N64_BUTTON_A) {
      gpio_put(LED_GPIO, 1);
    } else {
      gpio_put(LED_GPIO, 0);
    }

    // Rumble while A + B are held
    uint16_t ab = JOYBUS_N64_BUTTON_A | JOYBUS_N64_BUTTON_B;
    set_rumble((controller_state.buttons & ab) == ab);

    // Chill for a bit
    next_frame = delayed_by_ms(next_frame, POLL_INTERVAL_MS);
    sleep_until(next_frame);
  }

  return 0;
}
