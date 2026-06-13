#include <stdio.h>

#include <joybus/commands.h>
#include <joybus/errors.h>
#include <joybus/identify.h>
#include <joybus/target.h>
#include <joybus/common/gcn_controller.h>
#include <joybus/target/gcn_controller.h>

#include "unity.h"

#include "harness.h"

// The controller target under test
static struct joybus_target_gcn_controller controller;

// Spy for the reset callback
static int reset_count;
static void on_reset(struct joybus_target_gcn_controller *c)
{
  reset_count++;
}

// Spy for the motor state change callback
static int motor_count;
static uint8_t motor_last_state;
static void on_motor(struct joybus_target_gcn_controller *c, uint8_t state)
{
  motor_count++;
  motor_last_state = state;
}

/*
 * Fill the input state with field-unique, nibble-unique values so that a
 * packing mistake (swapped fields, wrong nibble, wrong shift) produces a
 * visible difference in the response bytes.
 */
static void set_distinct_input(void)
{
  controller.input.buttons       = JOYBUS_GCN_BUTTON_A | JOYBUS_GCN_BUTTON_Z;
  controller.input.stick_x       = 0x12;
  controller.input.stick_y       = 0x34;
  controller.input.substick_x    = 0x56;
  controller.input.substick_y    = 0x78;
  controller.input.trigger_left  = 0x9A;
  controller.input.trigger_right = 0xBC;
  controller.input.analog_a      = 0xDE;
  controller.input.analog_b      = 0xF1;
}

void setUp(void)
{
  // Recreate the controller from scratch; init establishes a complete known
  // state, including clearing any callbacks a previous test registered
  joybus_target_gcn_controller_init(&controller);

  // Point the harness at the controller and clear recorded responses
  harness_reset(JOYBUS_TARGET(&controller));

  // Reset the callback spies
  reset_count      = 0;
  motor_count      = 0;
  motor_last_state = 0xFF;
}

void tearDown(void)
{
}

// ---------------------------------------------------------------------------
// State API (no command bytes involved)
// ---------------------------------------------------------------------------

// Test the initial state set up by init: ID, centered origin, input = origin
static void test_init_defaults(void)
{
  // 0x09 0x00 = GCN device | standard controller, status clear
  uint8_t expected_id[] = {0x09, 0x00, 0x00};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_id, &controller.id, sizeof(expected_id));

  // Origin is centered sticks, everything else zero
  TEST_ASSERT_EQUAL_HEX16(0x0000, controller.origin.buttons);
  TEST_ASSERT_EQUAL_HEX8(0x80, controller.origin.stick_x);
  TEST_ASSERT_EQUAL_HEX8(0x80, controller.origin.stick_y);
  TEST_ASSERT_EQUAL_HEX8(0x80, controller.origin.substick_x);
  TEST_ASSERT_EQUAL_HEX8(0x80, controller.origin.substick_y);
  TEST_ASSERT_EQUAL_HEX8(0x00, controller.origin.trigger_left);
  TEST_ASSERT_EQUAL_HEX8(0x00, controller.origin.trigger_right);
  TEST_ASSERT_EQUAL_HEX8(0x00, controller.origin.analog_a);
  TEST_ASSERT_EQUAL_HEX8(0x00, controller.origin.analog_b);

  // Input starts as a copy of the origin, and is marked valid
  TEST_ASSERT_EQUAL_MEMORY(&controller.origin, &controller.input, sizeof(controller.input));
  TEST_ASSERT_TRUE(controller.input_valid);
}

// Test that a changed origin updates the analog fields and raises need-origin
static void test_set_origin_changed_sets_need_origin(void)
{
  struct joybus_gcn_controller_state new_origin = {
    .stick_x       = 0x81,
    .stick_y       = 0x82,
    .substick_x    = 0x83,
    .substick_y    = 0x84,
    .trigger_left  = 0x11,
    .trigger_right = 0x12,
  };
  joybus_target_gcn_controller_set_origin(&controller, &new_origin);

  // The six analog fields are copied into the origin
  TEST_ASSERT_EQUAL_HEX8(0x81, controller.origin.stick_x);
  TEST_ASSERT_EQUAL_HEX8(0x82, controller.origin.stick_y);
  TEST_ASSERT_EQUAL_HEX8(0x83, controller.origin.substick_x);
  TEST_ASSERT_EQUAL_HEX8(0x84, controller.origin.substick_y);
  TEST_ASSERT_EQUAL_HEX8(0x11, controller.origin.trigger_left);
  TEST_ASSERT_EQUAL_HEX8(0x12, controller.origin.trigger_right);

  // Need-origin is raised in both the input buttons and the ID status
  TEST_ASSERT_TRUE(controller.input.buttons & JOYBUS_GCN_NEED_ORIGIN);
  TEST_ASSERT_TRUE(joybus_id_get_status(&controller.id) & JOYBUS_STATUS_GCN_NEED_ORIGIN);
}

// Test that an unchanged origin does not raise need-origin
static void test_set_origin_unchanged_does_not_set_need_origin(void)
{
  // Same analog values the origin already holds after init
  struct joybus_gcn_controller_state same_origin = {
    .stick_x       = 0x80,
    .stick_y       = 0x80,
    .substick_x    = 0x80,
    .substick_y    = 0x80,
    .trigger_left  = 0x00,
    .trigger_right = 0x00,
  };
  joybus_target_gcn_controller_set_origin(&controller, &same_origin);

  TEST_ASSERT_FALSE(controller.input.buttons & JOYBUS_GCN_NEED_ORIGIN);
  TEST_ASSERT_FALSE(joybus_id_get_status(&controller.id) & JOYBUS_STATUS_GCN_NEED_ORIGIN);
}

// ---------------------------------------------------------------------------
// Identify (0x00)
// ---------------------------------------------------------------------------

// Test that identify responds at the first byte and does not mutate state
static void test_identify(void)
{
  uint8_t command[] = {JOYBUS_CMD_IDENTIFY};
  send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL(1, response.count);
  TEST_ASSERT_EQUAL(1, response.at_byte);
  TEST_ASSERT_EQUAL(JOYBUS_CMD_IDENTIFY_RX, response.len);

  uint8_t expected[] = {0x09, 0x00, 0x00};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));

  // A second identify returns the same answer
  send_command(command, sizeof(command));
  TEST_ASSERT_EQUAL(2, response.count);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));
}

// ---------------------------------------------------------------------------
// Reset (0xFF)
// ---------------------------------------------------------------------------

// Test that reset responds with the ID, fires the reset callback once, and stops the motor
static void test_reset(void)
{
  joybus_target_gcn_controller_set_reset_cb(&controller, on_reset);
  joybus_target_gcn_controller_set_motor_cb(&controller, on_motor);

  uint8_t command[] = {JOYBUS_CMD_RESET};
  send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL(1, response.count);
  TEST_ASSERT_EQUAL(1, response.at_byte);
  TEST_ASSERT_EQUAL(JOYBUS_CMD_RESET_RX, response.len);

  uint8_t expected[] = {0x09, 0x00, 0x00};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));

  TEST_ASSERT_EQUAL(1, reset_count);
  TEST_ASSERT_EQUAL(1, motor_count);
  TEST_ASSERT_EQUAL(JOYBUS_GCN_MOTOR_STOP, motor_last_state);
}

// Test that reset is safe when no callbacks are registered
static void test_reset_without_callbacks(void)
{
  uint8_t command[] = {JOYBUS_CMD_RESET};
  send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL(1, response.count);
  TEST_ASSERT_EQUAL(JOYBUS_CMD_RESET_RX, response.len);
}

// ---------------------------------------------------------------------------
// Read (0x40)
// ---------------------------------------------------------------------------

// Test that the read response is issued at the second command byte, before the motor byte arrives
static void test_read_responds_at_second_byte(void)
{
  uint8_t command[] = {JOYBUS_CMD_GCN_READ, JOYBUS_GCN_ANALOG_MODE_3, JOYBUS_GCN_MOTOR_STOP};
  send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL(1, response.count);
  TEST_ASSERT_EQUAL(2, response.at_byte);
  TEST_ASSERT_EQUAL(JOYBUS_CMD_GCN_READ_RX, response.len);
}

// Test the 8-byte input packing for every analog mode, including an out-of-range mode, against hand-computed golden
// bytes
static void test_read_pack_matrix(void)
{
  // All expected values are derived from the distinct input state:
  // buttons {0x01, 0x10}, stick 0x12/0x34, substick 0x56/0x78,
  // triggers 0x9A/0xBC, analog A/B 0xDE/0xF1
  static const struct {
    uint8_t mode;
    uint8_t expected[JOYBUS_CMD_GCN_READ_RX];
  } cases[] = {
    // Mode 0: substick full, triggers and analog A/B truncated to high nibbles
    {JOYBUS_GCN_ANALOG_MODE_0, {0x01, 0x10, 0x12, 0x34, 0x56, 0x78, 0x9B, 0xDF}},
    // Mode 1: triggers full, substick and analog A/B truncated
    {JOYBUS_GCN_ANALOG_MODE_1, {0x01, 0x10, 0x12, 0x34, 0x57, 0x9A, 0xBC, 0xDF}},
    // Mode 2: analog A/B full, substick and triggers truncated
    {JOYBUS_GCN_ANALOG_MODE_2, {0x01, 0x10, 0x12, 0x34, 0x57, 0x9B, 0xDE, 0xF1}},
    // Mode 3: substick and triggers full, analog A/B omitted
    {JOYBUS_GCN_ANALOG_MODE_3, {0x01, 0x10, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC}},
    // Mode 4: substick and analog A/B full, triggers omitted
    {JOYBUS_GCN_ANALOG_MODE_4, {0x01, 0x10, 0x12, 0x34, 0x56, 0x78, 0xDE, 0xF1}},
    // Out-of-range modes take the same path as mode 0
    {7, {0x01, 0x10, 0x12, 0x34, 0x56, 0x78, 0x9B, 0xDF}},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    // Each case runs against a freshly initialized controller, since a
    // completed read latches state that would bleed into the next case
    setUp();
    set_distinct_input();

    uint8_t command[] = {JOYBUS_CMD_GCN_READ, cases[i].mode, JOYBUS_GCN_MOTOR_STOP};
    send_command(command, sizeof(command));

    char msg[32];
    snprintf(msg, sizeof(msg), "analog mode %u", cases[i].mode);
    TEST_ASSERT_EQUAL_MESSAGE(JOYBUS_CMD_GCN_READ_RX, response.len, msg);
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(cases[i].expected, response.data, JOYBUS_CMD_GCN_READ_RX, msg);
  }
}

// Test that read falls back to the origin while the input is marked invalid
static void test_read_uses_origin_when_input_invalid(void)
{
  set_distinct_input();
  joybus_target_gcn_controller_input_valid(&controller, false);

  uint8_t command[] = {JOYBUS_CMD_GCN_READ, JOYBUS_GCN_ANALOG_MODE_3, JOYBUS_GCN_MOTOR_STOP};
  send_command(command, sizeof(command));

  // The default origin: no buttons, centered sticks, released triggers
  uint8_t expected[] = {0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));
}

// Test that a completed read latches use-origin, the analog mode, and the motor state
static void test_read_latches_flags(void)
{
  uint8_t command[] = {JOYBUS_CMD_GCN_READ, JOYBUS_GCN_ANALOG_MODE_2, JOYBUS_GCN_MOTOR_RUMBLE};
  send_command(command, sizeof(command));

  TEST_ASSERT_TRUE(controller.input.buttons & JOYBUS_GCN_USE_ORIGIN);

  uint8_t status = joybus_id_get_status(&controller.id);
  TEST_ASSERT_EQUAL_HEX8(JOYBUS_GCN_ANALOG_MODE_2, status & JOYBUS_STATUS_GCN_ANALOG_MODE_MASK);
  TEST_ASSERT_EQUAL_HEX8(JOYBUS_GCN_MOTOR_RUMBLE,
                         (status & JOYBUS_STATUS_GCN_MOTOR_STATE_MASK) >> JOYBUS_STATUS_GCN_MOTOR_STATE_SHIFT);
}

// Test that the motor callback fires only when the motor state changes
static void test_read_motor_callback_edge_triggered(void)
{
  joybus_target_gcn_controller_set_motor_cb(&controller, on_motor);

  uint8_t rumble[] = {JOYBUS_CMD_GCN_READ, JOYBUS_GCN_ANALOG_MODE_3, JOYBUS_GCN_MOTOR_RUMBLE};
  uint8_t stop[]   = {JOYBUS_CMD_GCN_READ, JOYBUS_GCN_ANALOG_MODE_3, JOYBUS_GCN_MOTOR_STOP};

  // Stop -> rumble fires the callback
  send_command(rumble, sizeof(rumble));
  TEST_ASSERT_EQUAL(1, motor_count);
  TEST_ASSERT_EQUAL(JOYBUS_GCN_MOTOR_RUMBLE, motor_last_state);

  // Rumble -> rumble does not
  send_command(rumble, sizeof(rumble));
  TEST_ASSERT_EQUAL(1, motor_count);

  // Rumble -> stop fires it again
  send_command(stop, sizeof(stop));
  TEST_ASSERT_EQUAL(2, motor_count);
  TEST_ASSERT_EQUAL(JOYBUS_GCN_MOTOR_STOP, motor_last_state);
}

// ---------------------------------------------------------------------------
// Read origin (0x41)
// ---------------------------------------------------------------------------

// Test that read origin responds with the origin state and clears need-origin
static void test_read_origin(void)
{
  struct joybus_gcn_controller_state new_origin = {
    .stick_x       = 0x81,
    .stick_y       = 0x82,
    .substick_x    = 0x83,
    .substick_y    = 0x84,
    .trigger_left  = 0x11,
    .trigger_right = 0x12,
  };
  joybus_target_gcn_controller_set_origin(&controller, &new_origin);

  uint8_t command[] = {JOYBUS_CMD_GCN_READ_ORIGIN};
  send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL(1, response.count);
  TEST_ASSERT_EQUAL(1, response.at_byte);
  TEST_ASSERT_EQUAL(JOYBUS_CMD_GCN_READ_ORIGIN_RX, response.len);

  // The origin's buttons and analog A/B are untouched by set_origin
  uint8_t expected[] = {0x00, 0x00, 0x81, 0x82, 0x83, 0x84, 0x11, 0x12, 0x00, 0x00};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));

  // Need-origin is cleared from both the input buttons and the ID status
  TEST_ASSERT_FALSE(controller.input.buttons & JOYBUS_GCN_NEED_ORIGIN);
  TEST_ASSERT_FALSE(joybus_id_get_status(&controller.id) & JOYBUS_STATUS_GCN_NEED_ORIGIN);
}

// ---------------------------------------------------------------------------
// Calibrate (0x42)
// ---------------------------------------------------------------------------

// Test that calibrate adopts the current input as the new origin and responds with it at the first byte
static void test_calibrate_copies_input_to_origin(void)
{
  set_distinct_input();

  uint8_t command[] = {JOYBUS_CMD_GCN_CALIBRATE, 0x00, 0x00};
  send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL(1, response.count);
  TEST_ASSERT_EQUAL(1, response.at_byte);
  TEST_ASSERT_EQUAL(JOYBUS_CMD_GCN_CALIBRATE_RX, response.len);

  // The response is the full input state, now serving as the origin
  uint8_t expected[] = {0x01, 0x10, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF1};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));
  TEST_ASSERT_EQUAL_MEMORY(&controller.input, &controller.origin, sizeof(controller.origin));
}

// Test that calibrate clears need-origin
static void test_calibrate_clears_need_origin(void)
{
  struct joybus_gcn_controller_state new_origin = {
    .stick_x       = 0x81,
    .stick_y       = 0x82,
    .substick_x    = 0x83,
    .substick_y    = 0x84,
    .trigger_left  = 0x11,
    .trigger_right = 0x12,
  };
  joybus_target_gcn_controller_set_origin(&controller, &new_origin);
  TEST_ASSERT_TRUE(joybus_id_get_status(&controller.id) & JOYBUS_STATUS_GCN_NEED_ORIGIN);

  uint8_t command[] = {JOYBUS_CMD_GCN_CALIBRATE, 0x00, 0x00};
  send_command(command, sizeof(command));

  TEST_ASSERT_FALSE(controller.input.buttons & JOYBUS_GCN_NEED_ORIGIN);
  TEST_ASSERT_FALSE(joybus_id_get_status(&controller.id) & JOYBUS_STATUS_GCN_NEED_ORIGIN);
}

// ---------------------------------------------------------------------------
// Read long (0x43)
// ---------------------------------------------------------------------------

// Test that read long responds at the second byte with the full 10-byte state
static void test_read_long_returns_full_state(void)
{
  set_distinct_input();

  uint8_t command[] = {JOYBUS_CMD_GCN_READ_LONG, JOYBUS_GCN_ANALOG_MODE_3, JOYBUS_GCN_MOTOR_STOP};
  send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL(1, response.count);
  TEST_ASSERT_EQUAL(2, response.at_byte);
  TEST_ASSERT_EQUAL(JOYBUS_CMD_GCN_READ_LONG_RX, response.len);

  uint8_t expected[] = {0x01, 0x10, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF1};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));
}

// Test that read long masks the analog mode to 3 bits and the motor state to 2 bits before latching them
static void test_read_long_masks_mode_and_motor(void)
{
  uint8_t command[] = {JOYBUS_CMD_GCN_READ_LONG, 0xFF, 0xFF};
  send_command(command, sizeof(command));

  uint8_t status = joybus_id_get_status(&controller.id);
  TEST_ASSERT_EQUAL_HEX8(0x07, status & JOYBUS_STATUS_GCN_ANALOG_MODE_MASK);
  TEST_ASSERT_EQUAL_HEX8(0x03, (status & JOYBUS_STATUS_GCN_MOTOR_STATE_MASK) >> JOYBUS_STATUS_GCN_MOTOR_STATE_SHIFT);
}

// ---------------------------------------------------------------------------
// Probe device (0x4D)
// ---------------------------------------------------------------------------

// Test that probe device responds with 8 zero bytes at the first byte
static void test_probe_device_responds_with_zeroes(void)
{
  // The trailing command bytes vary on real hardware; the handler ignores them
  uint8_t command[] = {JOYBUS_CMD_GCN_PROBE_DEVICE, 0x12, 0x34};
  send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL(1, response.count);
  TEST_ASSERT_EQUAL(1, response.at_byte);
  TEST_ASSERT_EQUAL(JOYBUS_CMD_GCN_PROBE_DEVICE_RX, response.len);

  uint8_t expected[JOYBUS_CMD_GCN_PROBE_DEVICE_RX] = {0};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));
}

// Test that probe device does not respond once wireless data has been received
static void test_probe_device_unsupported_after_wireless_received(void)
{
  joybus_id_set_type_flags(&controller.id, JOYBUS_TYPE_GCN_WIRELESS_RECEIVED);

  uint8_t command[] = {JOYBUS_CMD_GCN_PROBE_DEVICE, 0x12, 0x34};
  int result        = send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL(-JOYBUS_ERR_NOT_SUPPORTED, result);
  TEST_ASSERT_EQUAL(0, response.count);
}

// ---------------------------------------------------------------------------
// Fix device (0x4E)
// ---------------------------------------------------------------------------

// Test that fix device responds only at the third byte, extracts the 10-bit wireless ID, and updates the ID flags
static void test_fix_device(void)
{
  // Fix device is a WaveBird receiver command, so test against one
  joybus_target_gcn_controller_init_wavebird(&controller);

  // Wireless ID 0x2B1: top two bits in command[1], low byte in command[2]
  uint8_t command[] = {JOYBUS_CMD_GCN_FIX_DEVICE, 0x90, 0xB1};
  send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL(1, response.count);
  TEST_ASSERT_EQUAL(3, response.at_byte);
  TEST_ASSERT_EQUAL(JOYBUS_CMD_GCN_FIX_DEVICE_RX, response.len);

  // 0xAB 0x90 = wireless | no motor | GCN device | standard | wireless state
  //             | ID fixed, plus the wireless ID split across type and status
  uint8_t expected[] = {0xAB, 0x90, 0xB1};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));

  TEST_ASSERT_EQUAL_HEX16(0x2B1, joybus_target_gcn_controller_get_wireless_id(&controller));
  TEST_ASSERT_TRUE(joybus_target_gcn_controller_wireless_id_fixed(&controller));
}

// ---------------------------------------------------------------------------
// WaveBird receiver (wireless) lifecycle
// ---------------------------------------------------------------------------

// Test that a fresh WaveBird receiver identifies as wireless with no controller bound
static void test_wavebird_identify(void)
{
  joybus_target_gcn_controller_init_wavebird(&controller);

  uint8_t command[] = {JOYBUS_CMD_IDENTIFY};
  send_command(command, sizeof(command));

  uint8_t expected[] = {0xA8, 0x00, 0x00};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));
}

// Test that a received wireless ID is reflected in the identify response
static void test_wavebird_set_wireless_id(void)
{
  joybus_target_gcn_controller_init_wavebird(&controller);

  joybus_target_gcn_controller_set_wireless_id(&controller, 0x2B1);
  TEST_ASSERT_EQUAL_HEX16(0x2B1, joybus_target_gcn_controller_get_wireless_id(&controller));

  uint8_t command[] = {JOYBUS_CMD_IDENTIFY};
  send_command(command, sizeof(command));

  // 0xE9 0x80 = wireless | GCN device | standard, ID split across type and status
  uint8_t expected[] = {0xE9, 0x80, 0xB1};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));
}

// Test that setting the wireless ID again before it is fixed adopts the newer ID
static void test_wavebird_set_wireless_id_multiple(void)
{
  joybus_target_gcn_controller_init_wavebird(&controller);

  joybus_target_gcn_controller_set_wireless_id(&controller, 0x2B1);
  joybus_target_gcn_controller_set_wireless_id(&controller, 0x32F);
  TEST_ASSERT_EQUAL_HEX16(0x32F, joybus_target_gcn_controller_get_wireless_id(&controller));

  uint8_t command[] = {JOYBUS_CMD_IDENTIFY};
  send_command(command, sizeof(command));

  uint8_t expected[] = {0xE9, 0xC0, 0x2F};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));
}

// Test that fixing the wireless ID after a packet has been received marks it fixed in identify
static void test_wavebird_fix_device_after_wireless_id(void)
{
  joybus_target_gcn_controller_init_wavebird(&controller);
  joybus_target_gcn_controller_set_wireless_id(&controller, 0x2B1);

  // Before fixing, identify reports the wireless ID but not the fixed bit
  uint8_t identify[] = {JOYBUS_CMD_IDENTIFY};
  send_command(identify, sizeof(identify));
  uint8_t expected_before[] = {0xE9, 0x80, 0xB1};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_before, response.data, sizeof(expected_before));

  // Fixing the ID (wireless ID 0x2B1: top two bits in command[1], low byte in command[2])
  uint8_t fix[] = {JOYBUS_CMD_GCN_FIX_DEVICE, 0x90, 0xB1};
  send_command(fix, sizeof(fix));
  uint8_t expected_fix[] = {0xEB, 0x90, 0xB1};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_fix, response.data, sizeof(expected_fix));

  // A later identify reports the same fixed ID
  send_command(identify, sizeof(identify));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_fix, response.data, sizeof(expected_fix));
}

// Test that fixing the wireless ID before any packet is received still reports the fixed ID
static void test_wavebird_fix_device_without_wireless_id(void)
{
  joybus_target_gcn_controller_init_wavebird(&controller);

  uint8_t identify[] = {JOYBUS_CMD_IDENTIFY};
  send_command(identify, sizeof(identify));
  uint8_t expected_before[] = {0xA8, 0x00, 0x00};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_before, response.data, sizeof(expected_before));

  uint8_t fix[] = {JOYBUS_CMD_GCN_FIX_DEVICE, 0x90, 0xB1};
  send_command(fix, sizeof(fix));
  uint8_t expected_fix[] = {0xAB, 0x90, 0xB1};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_fix, response.data, sizeof(expected_fix));

  send_command(identify, sizeof(identify));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_fix, response.data, sizeof(expected_fix));
}

// Test that the wireless ID cannot be changed once it has been fixed
static void test_wavebird_set_wireless_id_ignored_when_fixed(void)
{
  joybus_target_gcn_controller_init_wavebird(&controller);
  joybus_target_gcn_controller_set_wireless_id(&controller, 0x2B1);

  uint8_t fix[] = {JOYBUS_CMD_GCN_FIX_DEVICE, 0x90, 0xB1};
  send_command(fix, sizeof(fix));

  // A later set is ignored now that the ID is fixed
  joybus_target_gcn_controller_set_wireless_id(&controller, 0x123);
  TEST_ASSERT_EQUAL_HEX16(0x2B1, joybus_target_gcn_controller_get_wireless_id(&controller));
}

// Test that setting an origin on a fixed WaveBird updates the origin and raises need-origin
static void test_wavebird_set_origin(void)
{
  joybus_target_gcn_controller_init_wavebird(&controller);
  joybus_target_gcn_controller_set_wireless_id(&controller, 0x2B1);

  uint8_t fix[] = {JOYBUS_CMD_GCN_FIX_DEVICE, 0x90, 0xB1};
  send_command(fix, sizeof(fix));

  struct joybus_gcn_controller_state origin = {
    .stick_x       = 0x85,
    .stick_y       = 0x86,
    .substick_x    = 0x87,
    .substick_y    = 0x88,
    .trigger_left  = 0x11,
    .trigger_right = 0x12,
  };
  joybus_target_gcn_controller_set_origin(&controller, &origin);

  TEST_ASSERT_EQUAL_HEX8(0x85, controller.origin.stick_x);
  TEST_ASSERT_EQUAL_HEX8(0x86, controller.origin.stick_y);
  TEST_ASSERT_EQUAL_HEX8(0x87, controller.origin.substick_x);
  TEST_ASSERT_EQUAL_HEX8(0x88, controller.origin.substick_y);
  TEST_ASSERT_EQUAL_HEX8(0x11, controller.origin.trigger_left);
  TEST_ASSERT_EQUAL_HEX8(0x12, controller.origin.trigger_right);

  // Identify now carries the fixed ID plus the need-origin status bit
  uint8_t identify[] = {JOYBUS_CMD_IDENTIFY};
  send_command(identify, sizeof(identify));
  uint8_t expected[] = {0xEB, 0xB0, 0xB1};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));
}

// Test that probe device stops responding once a wireless ID has been received
static void test_wavebird_probe_ignored_after_wireless_id(void)
{
  joybus_target_gcn_controller_init_wavebird(&controller);

  uint8_t probe[] = {JOYBUS_CMD_GCN_PROBE_DEVICE, 0x00, 0x00};
  send_command(probe, sizeof(probe));

  uint8_t expected[JOYBUS_CMD_GCN_PROBE_DEVICE_RX] = {0};
  TEST_ASSERT_EQUAL(JOYBUS_CMD_GCN_PROBE_DEVICE_RX, response.len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));

  // Once a wireless ID is received, probe is no longer supported
  joybus_target_gcn_controller_set_wireless_id(&controller, 0x2B1);
  int result = send_command(probe, sizeof(probe));
  TEST_ASSERT_EQUAL(-JOYBUS_ERR_NOT_SUPPORTED, result);
}

// ---------------------------------------------------------------------------
// Unsupported commands
// ---------------------------------------------------------------------------

// Test that an unknown command byte is rejected without a response
static void test_unknown_command_not_supported(void)
{
  uint8_t command[] = {0x99};
  int result        = send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL(-JOYBUS_ERR_NOT_SUPPORTED, result);
  TEST_ASSERT_EQUAL(0, response.count);
}

// ---------------------------------------------------------------------------
// Cross-command sequences
// ---------------------------------------------------------------------------

// Test the need-origin lifecycle: raised by new origin data, visible via identify, cleared by read origin
static void test_need_origin_lifecycle(void)
{
  uint8_t identify[]    = {JOYBUS_CMD_IDENTIFY};
  uint8_t read_origin[] = {JOYBUS_CMD_GCN_READ_ORIGIN};

  // A fresh controller reports a clear status byte
  send_command(identify, sizeof(identify));
  TEST_ASSERT_EQUAL_HEX8(0x00, response.data[2]);

  // New origin data raises need-origin
  struct joybus_gcn_controller_state new_origin = {
    .stick_x       = 0x81,
    .stick_y       = 0x82,
    .substick_x    = 0x83,
    .substick_y    = 0x84,
    .trigger_left  = 0x11,
    .trigger_right = 0x12,
  };
  joybus_target_gcn_controller_set_origin(&controller, &new_origin);
  send_command(identify, sizeof(identify));
  TEST_ASSERT_EQUAL_HEX8(JOYBUS_STATUS_GCN_NEED_ORIGIN, response.data[2]);

  // Reading the origin clears it again
  send_command(read_origin, sizeof(read_origin));
  send_command(identify, sizeof(identify));
  TEST_ASSERT_EQUAL_HEX8(0x00, response.data[2]);
}

// Test that the analog mode and motor state latched by a read are visible in a later identify response
static void test_mode_and_motor_latch_visible_in_identify(void)
{
  uint8_t read[]     = {JOYBUS_CMD_GCN_READ, JOYBUS_GCN_ANALOG_MODE_3, JOYBUS_GCN_MOTOR_RUMBLE};
  uint8_t identify[] = {JOYBUS_CMD_IDENTIFY};

  send_command(read, sizeof(read));
  send_command(identify, sizeof(identify));

  uint8_t expected_status = (JOYBUS_GCN_MOTOR_RUMBLE << JOYBUS_STATUS_GCN_MOTOR_STATE_SHIFT) | JOYBUS_GCN_ANALOG_MODE_3;
  TEST_ASSERT_EQUAL_HEX8(expected_status, response.data[2]);
}

int main(void)
{
  UNITY_BEGIN();

  // State API
  RUN_TEST(test_init_defaults);
  RUN_TEST(test_set_origin_changed_sets_need_origin);
  RUN_TEST(test_set_origin_unchanged_does_not_set_need_origin);

  // Identify
  RUN_TEST(test_identify);

  // Reset
  RUN_TEST(test_reset);
  RUN_TEST(test_reset_without_callbacks);

  // Read
  RUN_TEST(test_read_responds_at_second_byte);
  RUN_TEST(test_read_pack_matrix);
  RUN_TEST(test_read_uses_origin_when_input_invalid);
  RUN_TEST(test_read_latches_flags);
  RUN_TEST(test_read_motor_callback_edge_triggered);

  // Read origin
  RUN_TEST(test_read_origin);

  // Calibrate
  RUN_TEST(test_calibrate_copies_input_to_origin);
  RUN_TEST(test_calibrate_clears_need_origin);

  // Read long
  RUN_TEST(test_read_long_returns_full_state);
  RUN_TEST(test_read_long_masks_mode_and_motor);

  // Probe device
  RUN_TEST(test_probe_device_responds_with_zeroes);
  RUN_TEST(test_probe_device_unsupported_after_wireless_received);

  // Fix device
  RUN_TEST(test_fix_device);

  // WaveBird receiver (wireless) lifecycle
  RUN_TEST(test_wavebird_identify);
  RUN_TEST(test_wavebird_set_wireless_id);
  RUN_TEST(test_wavebird_set_wireless_id_multiple);
  RUN_TEST(test_wavebird_fix_device_after_wireless_id);
  RUN_TEST(test_wavebird_fix_device_without_wireless_id);
  RUN_TEST(test_wavebird_set_wireless_id_ignored_when_fixed);
  RUN_TEST(test_wavebird_set_origin);
  RUN_TEST(test_wavebird_probe_ignored_after_wireless_id);

  // Unsupported commands
  RUN_TEST(test_unknown_command_not_supported);

  // Cross-command sequences
  RUN_TEST(test_need_origin_lifecycle);
  RUN_TEST(test_mode_and_motor_latch_visible_in_identify);

  return UNITY_END();
}
