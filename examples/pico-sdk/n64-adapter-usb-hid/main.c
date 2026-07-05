#include <stdio.h>
#include <string.h>

#include "tusb.h"
#include "pico/stdlib.h"

#include <joybus/joybus.h>
#include "joybus/common/n64_controller.h"
#include "joybus/host/common.h"
#include <joybus/backend/rp2xxx.h>

#define JOYBUS_GPIO                 0
#define POLL_INTERVAL               1

// Joybus instance
static struct joybus_rp2xxx rp2xxx_bus;
static struct joybus *bus = JOYBUS(&rp2xxx_bus);

// Buffers for Joybus responses
static struct joybus_id id;
static struct joybus_n64_controller_state input;

// Buffer for building USB reports
static uint8_t report_buf[CFG_TUD_HID_EP_BUFSIZE];

// Polling mode
enum { POLL_MODE_IDENTIFY, POLL_MODE_READ };
static uint8_t poll_mode = POLL_MODE_IDENTIFY;

// Joybus identify callback
static void joybus_identify_cb(struct joybus *bus, int result, void *user_data)
{
  // Stay in identify mode on any Joybus error
  if (result < 0)
    return;

  // Check it's a N64 controller
  uint16_t type = id.type;
  if (!(type & JOYBUS_DEVICE_N64_CONTROLLER))
    return;

  // Move to polling for input
  poll_mode = POLL_MODE_READ;
}

// Joybus N64 read callback
static void joybus_read_cb(struct joybus *bus, int result, void *user_data)
{
  // Return to identify mode on any Joybus error
  if (result < 0)
    poll_mode = POLL_MODE_IDENTIFY;
}

// Map N64 buttons to HID gamepad buttons
static inline uint16_t get_buttons(const struct joybus_n64_controller_state *input)
{
  uint16_t hid_buttons = 0;

  // Use linux kernel "semantic" aliases where possible
  hid_buttons |= (input->buttons & JOYBUS_N64_BUTTON_A) ? GAMEPAD_BUTTON_A : 0;
  hid_buttons |= (input->buttons & JOYBUS_N64_BUTTON_B) ? GAMEPAD_BUTTON_B : 0;
  hid_buttons |= (input->buttons & JOYBUS_N64_BUTTON_START) ? GAMEPAD_BUTTON_START : 0;
  hid_buttons |= (input->buttons & JOYBUS_N64_BUTTON_Z) ? GAMEPAD_BUTTON_Z : 0;
  hid_buttons |= (input->buttons & JOYBUS_N64_BUTTON_L) ? GAMEPAD_BUTTON_TL : 0;
  hid_buttons |= (input->buttons & JOYBUS_N64_BUTTON_R) ? GAMEPAD_BUTTON_TR : 0;

  // No good semantic mapping for C buttons, so just map to high bits
  hid_buttons |= (input->buttons & JOYBUS_N64_BUTTON_C_UP) ? GAMEPAD_BUTTON_12 : 0;
  hid_buttons |= (input->buttons & JOYBUS_N64_BUTTON_C_DOWN) ? GAMEPAD_BUTTON_13 : 0;
  hid_buttons |= (input->buttons & JOYBUS_N64_BUTTON_C_LEFT) ? GAMEPAD_BUTTON_14 : 0;
  hid_buttons |= (input->buttons & JOYBUS_N64_BUTTON_C_RIGHT) ? GAMEPAD_BUTTON_15 : 0;

  return hid_buttons;
}

// Map N64 D-pad to HID hat values
static inline uint8_t get_hat(const struct joybus_n64_controller_state *input)
{
  bool up    = input->buttons & JOYBUS_N64_BUTTON_UP;
  bool down  = input->buttons & JOYBUS_N64_BUTTON_DOWN;
  bool left  = input->buttons & JOYBUS_N64_BUTTON_LEFT;
  bool right = input->buttons & JOYBUS_N64_BUTTON_RIGHT;

  if (up && right)
    return GAMEPAD_HAT_UP_RIGHT;
  if (right && down)
    return GAMEPAD_HAT_DOWN_RIGHT;
  if (down && left)
    return GAMEPAD_HAT_DOWN_LEFT;
  if (left && up)
    return GAMEPAD_HAT_UP_LEFT;
  if (up)
    return GAMEPAD_HAT_UP;
  if (right)
    return GAMEPAD_HAT_RIGHT;
  if (down)
    return GAMEPAD_HAT_DOWN;
  if (left)
    return GAMEPAD_HAT_LEFT;

  return GAMEPAD_HAT_CENTERED;
}

// Send HID reports at a regular interval
static bool poll_task(struct repeating_timer *timer)
{
  // Poll Joybus for data
  switch (poll_mode) {
    case POLL_MODE_IDENTIFY:
      joybus_identify_async(bus, &id, joybus_identify_cb, NULL);
      break;

    case POLL_MODE_READ:
      joybus_n64_read_async(bus, &input, joybus_read_cb, NULL);
      break;
  }

  // Send HID report
  if (tud_hid_ready()) {
    tud_hid_gamepad_report(0, input.stick_x, input.stick_y, 0, 0, 0, 0, get_hat(&input), get_buttons(&input));
  }

  return true;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen)
{
  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, const uint8_t *buffer,
                           uint16_t bufsize)
{
}

int main(void)
{
  // Initialize TinyUSB
  tusb_init();

  // Initialize Joybus
  joybus_rp2xxx_init(&rp2xxx_bus, JOYBUS_MODE_HOST, JOYBUS_GPIO, pio0);
  joybus_enable(bus);

  // Poll for Joybus data and send HID reports at regular intervals
  struct repeating_timer poll_timer;
  add_repeating_timer_ms(POLL_INTERVAL, poll_task, NULL, &poll_timer);

  // Handle USB events
  while (true)
    tud_task();

  return 0;
}
