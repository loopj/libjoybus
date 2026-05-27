#include <stdio.h>
#include <string.h>

#include "tusb.h"
#include "pico/stdlib.h"

#include <joybus/joybus.h>
#include <joybus/backend/rp2xxx.h>

#define JOYBUS_GPIO                 12
#define POLL_INTERVAL               1

// Joybus instance
static struct joybus_rp2xxx rp2xxx_bus;
static struct joybus *bus = JOYBUS(&rp2xxx_bus);

// Buffers for Joybus responses
static uint8_t joybus_response[JOYBUS_BLOCK_SIZE];
static struct joybus_gc_controller_input input;
static struct joybus_gc_controller_input origin;

// Buffer for building USB reports
static uint8_t report_buf[CFG_TUD_HID_EP_BUFSIZE];

// Polling mode
enum { POLL_MODE_IDENTIFY, POLL_MODE_READ };
static uint8_t poll_mode = POLL_MODE_IDENTIFY;

// Joybus GCN read origin callback
static void joybus_read_origin_cb(struct joybus *bus, int result, void *user_data)
{
  if (result < 0) {
    poll_mode = POLL_MODE_IDENTIFY;
    return;
  }

  memcpy(&origin, joybus_response, 10);
}

// Joybus identify callback
static void joybus_identify_cb(struct joybus *bus, int result, void *user_data)
{
  // Stay in identify mode on any Joybus error
  if (result < 0)
    return;

  // Check it's a GameCube controller
  uint16_t type = joybus_id_get_type(joybus_response);
  if (!(type & JOYBUS_ID_GCN_DEVICE))
    return;

  // Check we've received data if it's a wireless controller
  if ((type & JOYBUS_ID_GCN_WIRELESS) && !(type & JOYBUS_ID_GCN_WIRELESS_RECEIVED))
    return;

  // Check it's a standard controller
  if (!(type & JOYBUS_ID_GCN_STANDARD))
    return;

  // Read the origin
  joybus_gcn_read_origin(bus, joybus_response, joybus_read_origin_cb, NULL);

  // Move to polling for input
  poll_mode = POLL_MODE_READ;
}

// Joybus GCN read callback
static void joybus_read_cb(struct joybus *bus, int result, void *user_data)
{
  // Switch back to identify mode on any Joybus error
  if (result < 0) {
    poll_mode = POLL_MODE_IDENTIFY;
    return;
  }

  // Unpack the input state
  joybus_gcn_unpack_input(&input, joybus_response, JOYBUS_GCN_ANALOG_MODE_3);

  // If the "need origin" flag is set, fetch the origin state again
  if (input.buttons & JOYBUS_GCN_NEED_ORIGIN)
    joybus_gcn_read_origin(bus, joybus_response, joybus_read_origin_cb, NULL);
}

// Clamp an axis value based on the origin and expected resting position
static inline uint8_t clamp_axis(uint8_t value, uint8_t origin, uint8_t resting)
{
  int adjusted = value + (resting - origin);
  if (adjusted < 0)
    adjusted = 0;
  else if (adjusted > 255)
    adjusted = 255;

  return adjusted;
}

// Map GameCube analog stick values (0-255) to expected HID axis values (-128 to 127)
static inline int8_t get_stick(uint8_t value, uint8_t origin)
{
  return clamp_axis(value, origin, 128) - 128;
}

// Map GameCube trigger values (0-255) to expected HID axis values (-128 to 127)
static inline int8_t get_trigger(uint8_t value, uint8_t origin)
{
  return clamp_axis(value, origin, 0) - 128;
}

// Map GameCube buttons to HID gamepad buttons
static inline uint16_t get_buttons(const struct joybus_gc_controller_input *input)
{
  uint16_t hid_buttons = 0;

  hid_buttons |= (input->buttons & JOYBUS_GCN_BUTTON_A) ? GAMEPAD_BUTTON_A : 0;
  hid_buttons |= (input->buttons & JOYBUS_GCN_BUTTON_B) ? GAMEPAD_BUTTON_B : 0;
  hid_buttons |= (input->buttons & JOYBUS_GCN_BUTTON_X) ? GAMEPAD_BUTTON_X : 0;
  hid_buttons |= (input->buttons & JOYBUS_GCN_BUTTON_Y) ? GAMEPAD_BUTTON_Y : 0;
  hid_buttons |= (input->buttons & JOYBUS_GCN_BUTTON_START) ? GAMEPAD_BUTTON_START : 0;
  hid_buttons |= (input->buttons & JOYBUS_GCN_BUTTON_Z) ? GAMEPAD_BUTTON_Z : 0;
  hid_buttons |= (input->buttons & JOYBUS_GCN_BUTTON_L) ? GAMEPAD_BUTTON_TL : 0;
  hid_buttons |= (input->buttons & JOYBUS_GCN_BUTTON_R) ? GAMEPAD_BUTTON_TR : 0;

  return hid_buttons;
}

// Map GameCube D-pad to HID hat values
static inline uint8_t get_hat(const struct joybus_gc_controller_input *input)
{
  bool up    = input->buttons & JOYBUS_GCN_BUTTON_UP;
  bool down  = input->buttons & JOYBUS_GCN_BUTTON_DOWN;
  bool left  = input->buttons & JOYBUS_GCN_BUTTON_LEFT;
  bool right = input->buttons & JOYBUS_GCN_BUTTON_RIGHT;

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
      joybus_identify(bus, joybus_response, joybus_identify_cb, NULL);
      break;

    case POLL_MODE_READ:
      joybus_gcn_read(bus, JOYBUS_GCN_ANALOG_MODE_3, JOYBUS_GCN_MOTOR_STOP, joybus_response, joybus_read_cb, NULL);
      break;
  }

  // Send HID report
  if (tud_hid_ready()) {
    tud_hid_gamepad_report(
      0, get_stick(input.stick_x, origin.stick_x), get_stick(input.stick_y, origin.stick_y),
      get_stick(input.substick_x, origin.substick_x), get_stick(input.substick_y, origin.substick_y),
      get_trigger(input.trigger_left, origin.trigger_left), get_trigger(input.trigger_right, origin.trigger_right),
      get_hat(&input), get_buttons(&input));
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
  joybus_rp2xxx_init(&rp2xxx_bus, JOYBUS_GPIO, pio0);
  joybus_enable(bus);

  // Poll for Joybus data and send HID reports at regular intervals
  struct repeating_timer poll_timer;
  add_repeating_timer_ms(POLL_INTERVAL, poll_task, NULL, &poll_timer);

  // Handle USB events
  while (true)
    tud_task();

  return 0;
}