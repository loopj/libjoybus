#include <stdio.h>
#include <string.h>

#include "tusb.h"
#include "pico/stdlib.h"

#include <joybus/joybus.h>
#include "joybus/host/gcn.h"
#include <joybus/backend/rp2xxx.h>

#define JOYBUS_GPIO                 12
#define POLL_INTERVAL               1

// DualShock 4 report IDs
#define DS4_INPUT_REPORT_ID         0x01
#define DS4_OUTPUT_REPORT_ID        0x05

// DualShock 4 D-pad (hat) values
#define DS4_HAT_UP                  0x00
#define DS4_HAT_UP_RIGHT            0x01
#define DS4_HAT_RIGHT               0x02
#define DS4_HAT_DOWN_RIGHT          0x03
#define DS4_HAT_DOWN                0x04
#define DS4_HAT_DOWN_LEFT           0x05
#define DS4_HAT_LEFT                0x06
#define DS4_HAT_UP_LEFT             0x07
#define DS4_HAT_NEUTRAL             0x08

// Joybus instance
static struct joybus_rp2xxx rp2xxx_bus;
static struct joybus *bus = JOYBUS(&rp2xxx_bus);

// Buffers for Joybus responses
static struct joybus_id id;
static struct joybus_gcn_controller_state input;
static struct joybus_gcn_controller_state origin;

// Buffer for building USB reports
static uint8_t report_buf[CFG_TUD_HID_EP_BUFSIZE];

// 6-bit report counter included in every DS4 input report
static uint8_t report_counter = 0;

// Polling mode
enum { POLL_MODE_IDENTIFY, POLL_MODE_READ };
static uint8_t poll_mode = POLL_MODE_IDENTIFY;

// Rumble motor state, driven by DS4 output reports from the host
static uint8_t motor_state = JOYBUS_GCN_MOTOR_STOP;

// DualShock 4 feature reports the host GETs during init. macOS enumerates the
// controller from the descriptors and does not require the console
// authentication handshake, but returning valid calibration/version data avoids
// stalled control transfers. Bytes are the canned responses used by GP2040-CE.
static const uint8_t ds4_feature_calibration[] = { // report 0x02 (IMU calibration)
  0xfe, 0xff, 0x0e, 0x00, 0x04, 0x00, 0xd4, 0x22,
  0x2a, 0xdd, 0xbb, 0x22, 0x5e, 0xdd, 0x81, 0x22,
  0x84, 0xdd, 0x1c, 0x02, 0x1c, 0x02, 0x85, 0x1f,
  0xb0, 0xe0, 0xc6, 0x20, 0xb5, 0xe0, 0xb1, 0x20,
  0x83, 0xdf, 0x0c, 0x00
};

static const uint8_t ds4_feature_definition[] = { // report 0x03 (controller definition)
  0x21, 0x27, 0x04, 0xcf, 0x00, 0x2c, 0x56,
  0x08, 0x00, 0x3d, 0x00, 0xe8, 0x03, 0x04, 0x00,
  0xff, 0x7f, 0x0d, 0x0d, 0x00, 0x00, 0x00, 0x00,
  0x0d, 0x84, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t ds4_feature_version[] = { // report 0xA3 (firmware version/date)
  0x4a, 0x75, 0x6e, 0x20, 0x20, 0x39, 0x20, 0x32,
  0x30, 0x31, 0x37, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x31, 0x32, 0x3a, 0x33, 0x36, 0x3a, 0x34, 0x31,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x01, 0x08, 0xb4, 0x01, 0x00, 0x00, 0x00,
  0x07, 0xa0, 0x10, 0x20, 0x00, 0xa0, 0x02, 0x00
};

static const uint8_t ds4_feature_mac[] = { // report 0x12 (device/host MAC + BT class)
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // device MAC address
  0x08, 0x25, 0x00,                   // BT device class
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // host MAC address
};

// Joybus read origin callback
static void joybus_read_origin_cb(struct joybus *bus, int result, void *user_data)
{
  // Return to identify mode on any error
  if (result < 0)
    poll_mode = POLL_MODE_IDENTIFY;
}

// Joybus identify callback
static void joybus_identify_cb(struct joybus *bus, int result, void *user_data)
{
  // Stay in identify mode on any Joybus error
  if (result < 0)
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

  // Read the origin
  joybus_gcn_read_origin_async(bus, &origin, joybus_read_origin_cb, NULL);

  // Move to polling for input
  poll_mode = POLL_MODE_READ;
}

// Joybus GCN read callback
static void joybus_read_cb(struct joybus *bus, int result, void *user_data)
{
  // Return to identify mode on any error
  if (result < 0) {
    poll_mode = POLL_MODE_IDENTIFY;
    return;
  }

  // If the "need origin" flag is set, fetch the origin state again
  if (input.buttons & JOYBUS_GCN_NEED_ORIGIN)
    joybus_gcn_read_origin_async(bus, &origin, joybus_read_origin_cb, NULL);
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

// Map the GameCube D-pad to a DualShock 4 hat value
static uint8_t get_ds4_hat(const struct joybus_gcn_controller_state *input)
{
  bool up    = input->buttons & JOYBUS_GCN_BUTTON_UP;
  bool down  = input->buttons & JOYBUS_GCN_BUTTON_DOWN;
  bool left  = input->buttons & JOYBUS_GCN_BUTTON_LEFT;
  bool right = input->buttons & JOYBUS_GCN_BUTTON_RIGHT;

  if (up && right)
    return DS4_HAT_UP_RIGHT;
  if (right && down)
    return DS4_HAT_DOWN_RIGHT;
  if (down && left)
    return DS4_HAT_DOWN_LEFT;
  if (left && up)
    return DS4_HAT_UP_LEFT;
  if (up)
    return DS4_HAT_UP;
  if (right)
    return DS4_HAT_RIGHT;
  if (down)
    return DS4_HAT_DOWN;
  if (left)
    return DS4_HAT_LEFT;

  return DS4_HAT_NEUTRAL;
}

// Build a DualShock 4 input report (report 0x01) from the GameCube controller
// state. The GameCube layout is mapped onto the DS4 as:
//   A->Cross, B->Circle, X->Square, Y->Triangle, Start->Options, Z->R1,
//   L->L2 (analog + click), R->R2 (analog + click). L1/L3/R3/Share/PS/Touchpad
//   have no GameCube equivalent and are left unpressed.
static void build_ds4_report(void)
{
  uint16_t buttons = input.buttons;

  memset(report_buf, 0, sizeof(report_buf));
  report_buf[0] = DS4_INPUT_REPORT_ID;

  // Analog sticks (DS4 axes are unsigned, centered at 0x80). The GameCube
  // reports "up" as an increasing value, whereas the DS4 expects "up" to be
  // 0x00, so the Y axes are inverted.
  report_buf[1] = clamp_axis(input.stick_x, origin.stick_x, 0x80);
  report_buf[2] = 0xFF - clamp_axis(input.stick_y, origin.stick_y, 0x80);
  report_buf[3] = clamp_axis(input.substick_x, origin.substick_x, 0x80);
  report_buf[4] = 0xFF - clamp_axis(input.substick_y, origin.substick_y, 0x80);

  // D-pad (low nibble) plus the four face buttons (high nibble)
  report_buf[5] = get_ds4_hat(&input);
  if (buttons & JOYBUS_GCN_BUTTON_X)
    report_buf[5] |= 1 << 4; // Square
  if (buttons & JOYBUS_GCN_BUTTON_A)
    report_buf[5] |= 1 << 5; // Cross
  if (buttons & JOYBUS_GCN_BUTTON_B)
    report_buf[5] |= 1 << 6; // Circle
  if (buttons & JOYBUS_GCN_BUTTON_Y)
    report_buf[5] |= 1 << 7; // Triangle

  // Shoulder/trigger clicks and menu buttons
  if (buttons & JOYBUS_GCN_BUTTON_L)
    report_buf[6] |= 1 << 2; // L2 (click)
  if (buttons & JOYBUS_GCN_BUTTON_R)
    report_buf[6] |= 1 << 3; // R2 (click)
  if (buttons & JOYBUS_GCN_BUTTON_Z)
    report_buf[6] |= 1 << 1; // R1
  if (buttons & JOYBUS_GCN_BUTTON_START)
    report_buf[6] |= 1 << 5; // Options

  // 6-bit report counter (bits 2-7 of byte 7)
  report_buf[7] = (report_counter++ & 0x3F) << 2;

  // Analog triggers
  report_buf[8] = clamp_axis(input.trigger_left, origin.trigger_left, 0x00);
  report_buf[9] = clamp_axis(input.trigger_right, origin.trigger_right, 0x00);
}

// Send DS4 input reports at a regular interval
static bool poll_task(struct repeating_timer *timer)
{
  // Poll Joybus for data
  switch (poll_mode) {
    case POLL_MODE_IDENTIFY:
      joybus_identify_async(bus, &id, joybus_identify_cb, NULL);
      break;

    case POLL_MODE_READ:
      joybus_gcn_read_async(bus, JOYBUS_GCN_ANALOG_MODE_3, motor_state, &input, joybus_read_cb, NULL);
      break;
  }

  // Send a DS4 input report. The report ID is already in report_buf[0], so pass
  // report_id 0 to send the buffer verbatim.
  if (tud_hid_ready()) {
    build_ds4_report();
    tud_hid_report(0, report_buf, sizeof(report_buf));
  }

  return true;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen)
{
  if (report_type != HID_REPORT_TYPE_FEATURE)
    return 0;

  const uint8_t *src = NULL;
  uint16_t len       = 0;

  switch (report_id) {
    case 0x02:
      src = ds4_feature_calibration;
      len = sizeof(ds4_feature_calibration);
      break;
    case 0x03:
      src = ds4_feature_definition;
      len = sizeof(ds4_feature_definition);
      break;
    case 0xA3:
      src = ds4_feature_version;
      len = sizeof(ds4_feature_version);
      break;
    case 0x12:
      src = ds4_feature_mac;
      len = sizeof(ds4_feature_mac);
      break;
    default:
      return 0;
  }

  if (len > reqlen)
    len = reqlen;

  memcpy(buffer, src, len);
  return len;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, const uint8_t *buffer,
                           uint16_t bufsize)
{
  // Feature writes (e.g. the PS4 auth handshake) are not needed on macOS
  if (report_type == HID_REPORT_TYPE_FEATURE)
    return;

  uint8_t id          = report_id;
  const uint8_t *data = buffer;
  uint16_t len        = bufsize;

  // On the interrupt OUT endpoint TinyUSB delivers the whole report with the
  // report ID as the first byte and report_id == 0. Over the control endpoint
  // (SET_REPORT) the ID is stripped out and passed in report_id.
  if (id == 0 && len > 0) {
    id = data[0];
    data++;
    len--;
  }

  // The DS4 rumble/LED output report carries two motor magnitudes:
  //   data[3] = right (weak) motor, data[4] = left (strong) motor.
  // The GameCube motor is on/off only, so any non-zero magnitude turns it on.
  if (id == DS4_OUTPUT_REPORT_ID && len >= 5) {
    uint8_t weak   = data[3];
    uint8_t strong = data[4];
    motor_state    = (weak || strong) ? JOYBUS_GCN_MOTOR_RUMBLE : JOYBUS_GCN_MOTOR_STOP;
  }
}

int main(void)
{
  // Initialize TinyUSB
  tusb_init();

  // Initialize Joybus
  joybus_rp2xxx_init(&rp2xxx_bus, joybus_rp2xxx_config_default(JOYBUS_GPIO));
  joybus_enable(bus, JOYBUS_MODE_HOST);

  // Poll for Joybus data and send HID reports at regular intervals
  struct repeating_timer poll_timer;
  add_repeating_timer_ms(POLL_INTERVAL, poll_task, NULL, &poll_timer);

  // Handle USB events
  while (true)
    tud_task();

  return 0;
}
