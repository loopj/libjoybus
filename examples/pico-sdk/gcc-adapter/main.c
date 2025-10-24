#include <stdio.h>
#include <string.h>

#include "tusb.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"

#include <joybus/joybus.h>
#include <joybus/backend/rp2xxx.h>

#include "config.h"
#include "report_ids.h"

enum poll_mode { POLL_MODE_IDENTIFY, POLL_MODE_READ };

// Joybus GPIOs
static const uint JOYBUS_GPIOS[GCCA_JOYBUS_CHANNELS] = {
  GCCA_JOYBUS_GPIO_CH0,
  GCCA_JOYBUS_GPIO_CH1,
  GCCA_JOYBUS_GPIO_CH2,
  GCCA_JOYBUS_GPIO_CH3,
};

// Joybus instances
static struct joybus_rp2xxx rp2xxx_bus[GCCA_JOYBUS_CHANNELS];

// Buffers
static uint8_t joybus_response[GCCA_JOYBUS_CHANNELS][JOYBUS_BLOCK_SIZE];
static uint8_t report_buf[CFG_TUD_HID_EP_BUFSIZE];

// Adapter state
static bool polling_enabled = false;

// Controller state
static enum poll_mode poll_mode[GCCA_JOYBUS_CHANNELS];
static uint16_t controller_type[GCCA_JOYBUS_CHANNELS];
static struct joybus_gc_controller_input inputs[GCCA_JOYBUS_CHANNELS];
static struct joybus_gc_controller_input origins[GCCA_JOYBUS_CHANNELS];
static bool has_input_data[GCCA_JOYBUS_CHANNELS];
static uint8_t motor_state[GCCA_JOYBUS_CHANNELS];

// Reset bus state after error
static void reset_bus_state(int chan)
{
  poll_mode[chan]       = POLL_MODE_IDENTIFY;
  controller_type[chan] = 0;
  has_input_data[chan]  = false;
}

// Joybus read origin callback
static void joybus_read_origin_cb(struct joybus *bus, int result, void *user_data)
{
  // Determine which channel this callback is for
  int chan = (int)(intptr_t)user_data;

  if (result < 0) {
    reset_bus_state(chan);
    return;
  }

  memcpy(&origins[chan], joybus_response[chan], 10);
}

// Joybus identify callback
static void joybus_identify_cb(struct joybus *bus, int result, void *user_data)
{
  // Stay in identify mode on any Joybus error
  if (result < 0)
    return;

  // Determine which channel this callback is for
  int chan = (int)(intptr_t)user_data;

  // Check it's a GameCube controller
  uint16_t type = joybus_id_get_type(joybus_response[chan]);
  if (!(type & JOYBUS_ID_GCN_DEVICE))
    return;

  // Check we've received data if it's a wireless controller
  if ((type & JOYBUS_ID_GCN_WIRELESS) && !(type & JOYBUS_ID_GCN_WIRELESS_RECEIVED))
    return;

  // Check it's a standard controller
  if (!(type & JOYBUS_ID_GCN_STANDARD))
    return;

  // Save controller type
  controller_type[chan] = type;

  // Read the origin
  joybus_gcn_read_origin(bus, joybus_response[chan], joybus_read_origin_cb, (void *)(intptr_t)chan);

  // Move to read mode
  poll_mode[chan] = POLL_MODE_READ;
}

// Joybus read callback
static void joybus_read_cb(struct joybus *bus, int result, void *user_data)
{
  // Determine which channel this callback is for
  int chan = (int)(intptr_t)user_data;

  // Switch back to identify mode on any Joybus error
  if (result < 0) {
    reset_bus_state(chan);
    return;
  }

  // Unpack the input state
  joybus_gcn_unpack_input(&inputs[chan], joybus_response[chan], JOYBUS_GCN_ANALOG_MODE_3);

  // We have valid input data
  has_input_data[chan] = true;

  // If the "need origin" flag is set, fetch the origin state again
  if (inputs[chan].buttons & JOYBUS_GCN_NEED_ORIGIN)
    joybus_gcn_read_origin(bus, joybus_response[chan], joybus_read_origin_cb, (void *)(intptr_t)chan);
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

// Map Joybus GameCube button layout to GameCube adapter HID layout
static uint16_t get_buttons(uint16_t joybus_buttons)
{
  uint16_t hid_buttons = 0;
  hid_buttons |= joybus_buttons & 0x000F;        // A, B, X, Y
  hid_buttons |= (joybus_buttons & 0x0F00) >> 4; // D-pad
  hid_buttons |= (joybus_buttons & 0x0010) << 4; // Start
  hid_buttons |= (joybus_buttons & 0x7000) >> 3; // Z, R, L

  return hid_buttons;
}

// Send a report containing the current controller state
static void send_state_report(void)
{
  // Clear the report
  memset(report_buf, 0, RPT_STATE_LEN);

  // Set the report ID
  report_buf[0] = RPT_STATE;

  // Populate input data
  uint8_t *chan_buf = report_buf + 1;
  for (uint8_t chan = 0; chan < GCCA_JOYBUS_CHANNELS; chan++, chan_buf += 9) {
    // Set the controller type
    chan_buf[0] = controller_type[chan] & JOYBUS_ID_GCN_WIRELESS ? 0x22 : 0x10;

    // Set the rumble capability flag
    if (GCCA_RUMBLE_POWER_AVAILABLE)
      chan_buf[0] |= 0x04;

    if (!has_input_data[chan])
      continue;

    // Copy the button data
    uint16_t buttons = get_buttons(inputs[chan].buttons);
    chan_buf[1]      = buttons & 0xFF;
    chan_buf[2]      = (buttons >> 8) & 0xFF;

    // Copy the analog sticks and triggers, applying origin adjustments
    chan_buf[3] = clamp_axis(inputs[chan].stick_x, origins[chan].stick_x, 128);
    chan_buf[4] = clamp_axis(inputs[chan].stick_y, origins[chan].stick_y, 128);
    chan_buf[5] = clamp_axis(inputs[chan].substick_x, origins[chan].substick_x, 128);
    chan_buf[6] = clamp_axis(inputs[chan].substick_y, origins[chan].substick_y, 128);
    chan_buf[7] = clamp_axis(inputs[chan].trigger_left, origins[chan].trigger_left, 0);
    chan_buf[8] = clamp_axis(inputs[chan].trigger_right, origins[chan].trigger_right, 0);
  }

  // Send the report if the host is ready
  if (tud_hid_ready())
    tud_hid_report(0, report_buf, RPT_STATE_LEN);
}

// Send a HID report containing the "origin" positions of analog sticks and triggers
static void send_origin_report(void)
{
  // Set the report ID
  report_buf[0] = RPT_ORIGIN;

  // Populate the origin data from each channel
  uint8_t *chan_buf = report_buf + 1;
  for (uint8_t chan = 0; chan < GCCA_JOYBUS_CHANNELS; chan++, chan_buf += 6)
    memcpy(chan_buf, &origins[chan].stick_x, 6);

  // Send the report if the host is ready
  if (tud_hid_ready())
    tud_hid_report(0, report_buf, RPT_ORIGIN_LEN);
}

// Send a HID report acknowledging a "polling enabled" request
static void send_polling_enabled_report(void)
{
  // Build the report
  report_buf[0] = RPT_POLLING_ENABLED;
  report_buf[1] = !polling_enabled;

  // Send the report if the host is ready
  if (tud_hid_ready())
    tud_hid_report(0, report_buf, RPT_POLLING_ENABLED_LEN);
}

// Send a HID report acknowledging
static void send_ack_report(uint8_t report_id, bool success)
{
  // Build the report
  report_buf[0] = report_id;
  report_buf[1] = success;

  // Send the report if the host is ready
  if (tud_hid_ready())
    tud_hid_report(0, report_buf, RPT_POLLING_DISABLED_LEN);
}

// Handle an output report from the host
static void handle_output_report(uint8_t report_id, uint8_t const *data, uint16_t length)
{
  switch (report_id) {
    case RPT_SET_MOTOR:
      // Save the motor state
      for (int i = 0; i < GCCA_JOYBUS_CHANNELS; i++)
        motor_state[i] = data[i];
      break;

    case RPT_GET_ORIGIN:
      // Send origin report
      send_origin_report();
      break;

    case RPT_ENABLE_POLLING:
      // Send polling enabled report
      send_ack_report(RPT_POLLING_ENABLED, !polling_enabled);

      // Mark polling as enabled
      polling_enabled = true;
      break;

    case RPT_DISABLE_POLLING: {
      // Send polling disabled report
      send_ack_report(RPT_POLLING_DISABLED, polling_enabled);

      // Mark polling as disabled
      polling_enabled = false;
      break;
    }

    case RPT_RESET:
      // Reboot the device
      watchdog_reboot(0, 0, 0);
      while (1)
        ;

      break;
  }
}

// Poll for Joybus data and send HID reports
static bool poll_task(struct repeating_timer *timer)
{
  if (!polling_enabled)
    return true;

  // Poll Joybus channels for input data
  for (int i = 0; i < GCCA_JOYBUS_CHANNELS; i++) {
    switch (poll_mode[i]) {
      case POLL_MODE_IDENTIFY:
        joybus_identify(JOYBUS(&rp2xxx_bus[i]), joybus_response[i], joybus_identify_cb, (void *)(intptr_t)i);
        break;

      case POLL_MODE_READ:
        joybus_gcn_read(JOYBUS(&rp2xxx_bus[i]), JOYBUS_GCN_ANALOG_MODE_3, motor_state[i], joybus_response[i],
                        joybus_read_cb, (void *)(intptr_t)i);
        break;
    }
  }

  // Make a new "state" HID report available
  send_state_report();

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
  if (report_type == HID_REPORT_TYPE_OUTPUT)
    handle_output_report(buffer[0], buffer + 1, bufsize - 1);
}

int main(void)
{
  // Initialize TinyUSB
  tusb_init();

  // Initialize Joybus channels
  for (int i = 0; i < GCCA_JOYBUS_CHANNELS; i++) {
    joybus_rp2xxx_init(&rp2xxx_bus[i], JOYBUS_GPIOS[i], pio0);
    joybus_enable(JOYBUS(&rp2xxx_bus[i]));

    reset_bus_state(i);
  }

  // Poll for Joybus data and send HID reports at regular intervals
  struct repeating_timer poll_timer;
  add_repeating_timer_ms(GCCA_POLL_INTERVAL, poll_task, NULL, &poll_timer);

  // Handle USB events
  while (true)
    tud_task();

  return 0;
}