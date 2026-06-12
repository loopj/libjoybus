#include <string.h>

#include <joybus/bus.h>
#include <joybus/identify.h>
#include "joybus/common/gcn_controller.h"
#include <joybus/backend/loopback.h>
#include <joybus/host/common.h>
#include <joybus/host/gcn.h>
#include <joybus/target/gcn_controller.h>

#include "unity.h"

// A loopback Joybus and a WaveBird controller target`
struct joybus bus;
struct joybus_target_gcn_controller controller;

void setUp(void)
{
  // Initialize the loopback bus
  joybus_loopback_init(&bus);
  joybus_enable(&bus);

  // Register a standard GameCube controller target
  joybus_target_gcn_controller_init(&controller);
  joybus_target_register(&bus, JOYBUS_TARGET(&controller));
}

void tearDown(void)
{
}

// Test that the "identify" response is correct for a standard GameCube controller
static void test_gcn_controller_identify()
{
  // Send an identify command
  struct joybus_id id;
  joybus_identify(&bus, &id);

  // Test device identify response is as expected
  uint8_t expected[] = {0x09, 0x00, 0x00};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, &id, sizeof(id));
}

// Test that the "need origin" flag is cleared after a "read origin" command
static void test_gcn_controller_identify_after_read_origin()
{
  struct joybus_id id;
  struct joybus_gcn_controller_state origin;

  // Set an origin
  struct joybus_gcn_controller_state new_origin = {
    .stick_x       = 0x81,
    .stick_y       = 0x82,
    .substick_x    = 0x83,
    .substick_y    = 0x84,
    .trigger_left  = 0x11,
    .trigger_right = 0x12,
  };
  joybus_target_gcn_controller_set_origin(&controller, &new_origin);

  // Send an identify command
  joybus_identify(&bus, &id);

  // Test device identify response is as expected
  uint8_t expected_response[] = {0x09, 0x00, 0x20};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_response, &id, sizeof(id));

  // Send a read origin command
  joybus_gcn_read_origin(&bus, &origin);

  // Send another identify command
  joybus_identify(&bus, &id);

  // Verify the "need_origin" flag is no longer set
  uint8_t expected_response_2[] = {0x09, 0x00, 0x00};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_response_2, &id, sizeof(id));
}

// Test that "analog mode" and "motor state" are saved after a "read" command
static void test_gcn_controller_identify_after_read()
{
  struct joybus_id id;
  struct joybus_gcn_controller_state origin;
  struct joybus_gcn_controller_state input;

  // Send a read origin command
  joybus_gcn_read_origin(&bus, &origin);

  // Send a read command
  joybus_gcn_read(&bus, JOYBUS_GCN_ANALOG_MODE_3, JOYBUS_GCN_MOTOR_RUMBLE, &input);

  // Send an identify command
  joybus_identify(&bus, &id);

  // Verify the "analog_mode" and "motor_state" are present
  uint8_t expected_response[] = {0x09, 0x00, 0x0B};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_response, &id, sizeof(id));
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();

  RUN_TEST(test_gcn_controller_identify);
  RUN_TEST(test_gcn_controller_identify_after_read_origin);
  RUN_TEST(test_gcn_controller_identify_after_read);

  return UNITY_END();
}
