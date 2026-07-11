#include <driver/gpio.h>
#include <esp_timer.h>

#include <joybus/joybus.h>
#include <joybus/backend/esp32.h>

// Change these defines to match your hardware setup
#define JOYBUS_GPIO      1
#define LED_GPIO         2

#define POLL_INTERVAL_MS 15

// Joybus instance
static struct joybus_esp32 esp32_bus;
static struct joybus *bus = JOYBUS(&esp32_bus);

// Joybus state
static struct joybus_id id;
static struct joybus_gcn_controller_state controller_state;

// Polling mode
enum { POLL_MODE_IDENTIFY, POLL_MODE_READ };
static uint8_t poll_mode = POLL_MODE_IDENTIFY;

// Requested motor state, updated from the button state on each read
static enum joybus_gcn_motor_state motor = JOYBUS_GCN_MOTOR_STOP;

static void joybus_identify_cb(struct joybus *bus, int status, void *user_data)
{
  // Stay in identify mode on any Joybus error
  if (status < 0)
    return;

  // Check it's a GameCube controller
  uint16_t type = id.type;
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

static void joybus_read_cb(struct joybus *bus, int status, void *user_data)
{
  // Switch back to identify mode on any Joybus error
  if (status < 0) {
    poll_mode = POLL_MODE_IDENTIFY;
    motor     = JOYBUS_GCN_MOTOR_STOP;
    return;
  }

  // Light the LED while A is held
  gpio_set_level(LED_GPIO, (controller_state.buttons & JOYBUS_GCN_BUTTON_A) ? 1 : 0);

  // Rumble the motor while A + B are held
  uint16_t ab = JOYBUS_GCN_BUTTON_A | JOYBUS_GCN_BUTTON_B;
  motor       = (controller_state.buttons & ab) == ab ? JOYBUS_GCN_MOTOR_RUMBLE : JOYBUS_GCN_MOTOR_STOP;
}

// Poll the Joybus at regular intervals
static void poll_task(void *arg)
{
  switch (poll_mode) {
    case POLL_MODE_IDENTIFY:
      gpio_set_level(LED_GPIO, 0);
      joybus_identify_async(bus, &id, joybus_identify_cb, NULL);
      break;

    case POLL_MODE_READ:
      joybus_gcn_read_async(bus, JOYBUS_GCN_ANALOG_MODE_3, motor, &controller_state, joybus_read_cb, NULL);
      break;
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

  // Poll for Joybus data at regular intervals
  const esp_timer_create_args_t poll_args = {.callback = poll_task, .name = "poll"};
  esp_timer_handle_t poll_timer;
  esp_timer_create(&poll_args, &poll_timer);
  esp_timer_start_periodic(poll_timer, POLL_INTERVAL_MS * 1000);
}
