#include "em_cmu.h"
#include "em_gpio.h"

#include "sl_sleeptimer.h"

#include <joybus/joybus.h>
#include <joybus/backend/gecko.h>

// Change these defines to match your hardware setup
#define JOYBUS_PORT             gpioPortD
#define JOYBUS_PIN              3
#define LED_PORT                gpioPortA
#define LED_PIN                 4

#define JOYBUS_POLL_INTERVAL_MS 15

// Joybus instance
struct joybus_gecko gecko_bus;
struct joybus *bus = JOYBUS(&gecko_bus);

// Joybus state
static struct joybus_id id;
static struct joybus_gcn_controller_state input;

// Polling mode
enum { POLL_MODE_IDENTIFY, POLL_MODE_READ };
static uint8_t poll_mode = POLL_MODE_IDENTIFY;

// Sleeptimer handle for polling
static sl_sleeptimer_timer_handle_t poll_timer;

void joybus_identify_cb(struct joybus *bus, int status, void *user_data)
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

void joybus_read_cb(struct joybus *bus, int status, void *user_data)
{
  // Switch back to identify mode on any Joybus error
  if (status < 0) {
    poll_mode = POLL_MODE_IDENTIFY;
    return;
  }

  // Light the LED while the A button is held
  if (input.buttons & JOYBUS_GCN_BUTTON_A) {
    GPIO_PinOutSet(LED_PORT, LED_PIN);
  } else {
    GPIO_PinOutClear(LED_PORT, LED_PIN);
  }
}

// Poll Joybus for data
static void poll_task(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  switch (poll_mode) {
    case POLL_MODE_IDENTIFY:
      joybus_identify_async(bus, &id, joybus_identify_cb, NULL);
      break;

    case POLL_MODE_READ:
      joybus_gcn_read_async(bus, JOYBUS_GCN_ANALOG_MODE_3, JOYBUS_GCN_MOTOR_STOP, &input, joybus_read_cb, NULL);
      break;
  }
}

void app_init(void)
{
  // Set up GPIO for status LED
  CMU_ClockEnable(cmuClock_GPIO, true);
  GPIO_PinModeSet(LED_PORT, LED_PIN, gpioModePushPull, 0);

  // Initialize Joybus
  joybus_gecko_init(&gecko_bus, joybus_gecko_config_default(JOYBUS_PORT, JOYBUS_PIN));
  joybus_enable(bus, JOYBUS_MODE_HOST);

  // Poll for Joybus data at regular intervals
  sl_sleeptimer_init();
  sl_sleeptimer_start_periodic_timer(&poll_timer, sl_sleeptimer_ms_to_tick(JOYBUS_POLL_INTERVAL_MS), poll_task, bus, 0,
                                     SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG);
}

void app_process_action(void)
{
  // Nothing to do here, everything is handled in the timer callback
}
