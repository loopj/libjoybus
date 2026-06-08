#include <stdbool.h>
#include <string.h>

#include <joybus/bus.h>
#include <joybus/checksum.h>
#include <joybus/commands.h>
#include <joybus/backend/loopback.h>
#include <joybus/host/common.h>
#include <joybus/host/n64.h>
#include <joybus/target/n64_controller.h>
#include <joybus/target/n64_rumble_pak.h>

#include "unity.h"

// A loopback Joybus, an N64 controller, and a rumble pak
struct joybus bus;
struct joybus_n64_controller controller;
struct joybus_n64_rumble_pak rumble_pak;

// Spy callback to capture responses
static uint8_t response[JOYBUS_BLOCK_SIZE];
static int response_len;
static void spy(struct joybus *bus, int result, void *user_data)
{
  response_len = result;
}

// Track motor callback invocations
static int motor_callback_count;
static bool last_motor_state;
static void on_motor_change(struct joybus_n64_rumble_pak *pak, bool active)
{
  motor_callback_count++;
  last_motor_state = active;
}

// Track accessory detection results
static int detect_callback_count;
static int detected_accessory_type;
static void on_accessory_detected(int accessory_type, void *user_data)
{
  detect_callback_count++;
  detected_accessory_type = accessory_type;
}

void setUp(void)
{
  // Initialize the loopback bus
  joybus_loopback_init(&bus);
  joybus_enable(&bus);

  // Initialize the controller and rumble pak, attach the pak, power on
  joybus_n64_controller_init(&controller);
  joybus_n64_rumble_pak_init(&rumble_pak);
  joybus_n64_controller_attach_accessory(&controller, JOYBUS_N64_ACCESSORY(&rumble_pak));
  joybus_target_register(&bus, JOYBUS_TARGET(&controller));

  // Bring up communication so accessory reads/writes are honored
  joybus_identify(&bus, response, spy, NULL);

  // Wire up the motor callback
  joybus_n64_rumble_pak_set_motor_callback(&rumble_pak, on_motor_change);

  // Reset response capture
  response_len = -1;
  memset(response, 0, sizeof(response));

  // Reset motor tracking
  motor_callback_count = 0;
  last_motor_state     = false;

  // Reset detection tracking
  detect_callback_count   = 0;
  detected_accessory_type = -1;
}

void tearDown(void)
{
}

// Test that a read from the probe region returns the rumble pak signature (0x80 x 32) plus valid CRC
static void test_n64_rumble_pak_probe_read_returns_signature()
{
  joybus_n64_accessory_read(&bus, 0x8000, response, spy, NULL);

  uint8_t expected[JOYBUS_CMD_N64_ACCESSORY_READ_RX];
  memset(expected, 0x80, JOYBUS_ACCESSORY_BLOCK_SIZE);
  expected[JOYBUS_ACCESSORY_BLOCK_SIZE] = joybus_data_checksum(expected, JOYBUS_ACCESSORY_BLOCK_SIZE);

  TEST_ASSERT_EQUAL(JOYBUS_CMD_N64_ACCESSORY_READ_RX, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response, JOYBUS_CMD_N64_ACCESSORY_READ_RX);
}

// Test that the OEM detection sequence (write 0xFE to probe, then read) returns the rumble pak signature
static void test_n64_rumble_pak_detection_sequence()
{
  uint8_t probe_write[JOYBUS_ACCESSORY_BLOCK_SIZE];
  memset(probe_write, 0xFE, sizeof(probe_write));
  joybus_n64_accessory_write(&bus, 0x8000, probe_write, response, spy, NULL);

  joybus_n64_accessory_read(&bus, 0x8000, response, spy, NULL);

  uint8_t expected[JOYBUS_CMD_N64_ACCESSORY_READ_RX];
  memset(expected, 0x80, JOYBUS_ACCESSORY_BLOCK_SIZE);
  expected[JOYBUS_ACCESSORY_BLOCK_SIZE] = joybus_data_checksum(expected, JOYBUS_ACCESSORY_BLOCK_SIZE);

  TEST_ASSERT_EQUAL(JOYBUS_CMD_N64_ACCESSORY_READ_RX, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response, JOYBUS_CMD_N64_ACCESSORY_READ_RX);
}

// Test that motor_start fires the callback once with active=true
static void test_n64_rumble_pak_motor_start_fires_callback()
{
  joybus_n64_motor_start(&bus);

  TEST_ASSERT_EQUAL(1, motor_callback_count);
  TEST_ASSERT_TRUE(last_motor_state);
  TEST_ASSERT_TRUE(rumble_pak.active);
}

// Test that motor_stop after motor_start fires the callback with active=false
static void test_n64_rumble_pak_motor_stop_fires_callback_after_start()
{
  joybus_n64_motor_start(&bus);
  joybus_n64_motor_stop(&bus);

  TEST_ASSERT_EQUAL(2, motor_callback_count);
  TEST_ASSERT_FALSE(last_motor_state);
  TEST_ASSERT_FALSE(rumble_pak.active);
}

// Test that consecutive motor_start writes only fire the callback once (state unchanged)
static void test_n64_rumble_pak_motor_callback_only_on_state_change()
{
  joybus_n64_motor_start(&bus);
  joybus_n64_motor_start(&bus);
  joybus_n64_motor_start(&bus);

  TEST_ASSERT_EQUAL(1, motor_callback_count);
}

// Test that motor_stop with no prior start does not fire the callback (motor already off)
static void test_n64_rumble_pak_motor_stop_when_already_off()
{
  joybus_n64_motor_stop(&bus);

  TEST_ASSERT_EQUAL(0, motor_callback_count);
  TEST_ASSERT_FALSE(rumble_pak.active);
}

// Test that motor writes are silently dropped when no callback is registered
static void test_n64_rumble_pak_no_callback_set()
{
  joybus_n64_rumble_pak_set_motor_callback(&rumble_pak, NULL);

  joybus_n64_motor_start(&bus);
  joybus_n64_motor_stop(&bus);

  // Internal state still updates, but no callback fires
  TEST_ASSERT_EQUAL(0, motor_callback_count);
  TEST_ASSERT_FALSE(rumble_pak.active);
}

// Test that writes to the probe region do not toggle the motor state
static void test_n64_rumble_pak_write_outside_motor_region_ignored()
{
  uint8_t data[JOYBUS_ACCESSORY_BLOCK_SIZE];
  memset(data, 0x01, sizeof(data));
  joybus_n64_accessory_write(&bus, 0x8000, data, response, spy, NULL);

  TEST_ASSERT_EQUAL(0, motor_callback_count);
  TEST_ASSERT_FALSE(rumble_pak.active);
}

// Test that the host accessory detection routine identifies the pak as a rumble pak
static void test_n64_rumble_pak_accessory_detect_identifies_rumble_pak()
{
  joybus_n64_accessory_detect(&bus, on_accessory_detected, NULL);

  TEST_ASSERT_EQUAL(1, detect_callback_count);
  TEST_ASSERT_EQUAL(JOYBUS_N64_ACCESSORY_RUMBLE_PAK, detected_accessory_type);
}

// Test that reads from outside the probe region return zeros
static void test_n64_rumble_pak_read_outside_probe_region_returns_zeros()
{
  joybus_n64_accessory_read(&bus, 0x0000, response, spy, NULL);

  uint8_t expected[JOYBUS_CMD_N64_ACCESSORY_READ_RX] = {0};
  expected[JOYBUS_ACCESSORY_BLOCK_SIZE]              = joybus_data_checksum(expected, JOYBUS_ACCESSORY_BLOCK_SIZE);

  TEST_ASSERT_EQUAL(JOYBUS_CMD_N64_ACCESSORY_READ_RX, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response, JOYBUS_CMD_N64_ACCESSORY_READ_RX);
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();
  RUN_TEST(test_n64_rumble_pak_probe_read_returns_signature);
  RUN_TEST(test_n64_rumble_pak_detection_sequence);
  RUN_TEST(test_n64_rumble_pak_motor_start_fires_callback);
  RUN_TEST(test_n64_rumble_pak_motor_stop_fires_callback_after_start);
  RUN_TEST(test_n64_rumble_pak_motor_callback_only_on_state_change);
  RUN_TEST(test_n64_rumble_pak_motor_stop_when_already_off);
  RUN_TEST(test_n64_rumble_pak_no_callback_set);
  RUN_TEST(test_n64_rumble_pak_write_outside_motor_region_ignored);
  RUN_TEST(test_n64_rumble_pak_read_outside_probe_region_returns_zeros);
  RUN_TEST(test_n64_rumble_pak_accessory_detect_identifies_rumble_pak);

  return UNITY_END();
}
