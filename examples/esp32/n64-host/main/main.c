#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <joybus/joybus.h>
#include <joybus/backend/esp32.h>
#include <joybus/host/n64_rumble_pak.h>

#define JOYBUS_GPIO          1
#define LED_GPIO             2

#define POLL_INTERVAL_MS     16
#define IDENTIFY_INTERVAL_MS 2000

// Joybus instance
static struct joybus_esp32 esp32_bus;
static struct joybus *bus = JOYBUS(&esp32_bus);

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
  static TickType_t next_identify = 0;
  if (!controller_present || xTaskGetTickCount() >= next_identify) {
    next_identify = xTaskGetTickCount() + pdMS_TO_TICKS(IDENTIFY_INTERVAL_MS);

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

void app_main(void)
{
  // Set up GPIO for status LED
  gpio_config_t led = {.pin_bit_mask = 1ULL << LED_GPIO, .mode = GPIO_MODE_OUTPUT};
  gpio_config(&led);
  gpio_set_level(LED_GPIO, 0);

  // Initialize Joybus
  joybus_esp32_init(&esp32_bus, joybus_esp32_config_default(JOYBUS_GPIO));
  joybus_enable(bus, JOYBUS_MODE_HOST);

  // Poll the controller once per "frame"
  while (1) {
    poll_controller();

    // Light the LED while A is held
    if (controller_state.buttons & JOYBUS_N64_BUTTON_A) {
      gpio_set_level(LED_GPIO, 1);
    } else {
      gpio_set_level(LED_GPIO, 0);
    }

    // Rumble while A + B are held
    uint16_t ab = JOYBUS_N64_BUTTON_A | JOYBUS_N64_BUTTON_B;
    set_rumble((controller_state.buttons & ab) == ab);

    // Chill for a bit
    vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
  }
}
