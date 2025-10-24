#include <string.h>

#include <joybus/bus.h>
#include <joybus/commands.h>
#include <joybus/backend/loopback.h>
#include <joybus/host/common.h>
#include <joybus/host/gamecube.h>
#include <joybus/target/gc_controller.h>

#include "unity.h"

// A loopback Joybus and a WaveBird controller target`
struct joybus bus;
struct joybus_gc_controller controller;

// Spy callback to capture responses
static uint8_t response[JOYBUS_BLOCK_SIZE];
static int response_len;
static void spy(struct joybus *bus, int result, void *user_data)
{
  response_len = result;
}

void setUp(void)
{
  // Initialize the loopback bus
  joybus_loopback_init(&bus);
  joybus_enable(&bus);

  // Register a standard GameCube controller target
  joybus_gc_controller_init(&controller, JOYBUS_GAMECUBE_CONTROLLER);
  joybus_target_register(&bus, JOYBUS_TARGET(&controller));

  // Reset response capture
  response_len = -1;
  memset(response, 0, sizeof(response));
}

void tearDown(void)
{
}

// Test that the "identify" response is correct for a standard GameCube controller
static void test_gc_controller_identify()
{
  // Send an identify command
  joybus_identify(&bus, response, spy, NULL);

  // Test device identify response is as expected
  uint8_t expected[] = {0x09, 0x00, 0x00};
  TEST_ASSERT_EQUAL(3, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response, 3);
}

// Test that the "need origin" flag is cleared after a "read origin" command
static void test_gc_controller_identify_after_read_origin()
{
  // Set an origin
  struct joybus_gc_controller_input new_origin = {
    .stick_x       = 0x81,
    .stick_y       = 0x82,
    .substick_x    = 0x83,
    .substick_y    = 0x84,
    .trigger_left  = 0x11,
    .trigger_right = 0x12,
  };
  joybus_gc_controller_set_origin(&controller, &new_origin);

  // Send an identify command
  joybus_identify(&bus, response, spy, NULL);

  // Test device identify response is as expected
  uint8_t expected_response[] = {0x09, 0x00, 0x20};
  TEST_ASSERT_EQUAL(3, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_response, response, 3);

  // Send a read origin command
  joybus_gcn_read_origin(&bus, response, spy, NULL);

  // Send another identify command
  joybus_identify(&bus, response, spy, NULL);

  // Verify the "need_origin" flag is no longer set
  uint8_t expected_response_2[] = {0x09, 0x00, 0x00};
  TEST_ASSERT_EQUAL(3, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_response_2, response, 3);
}

// Test that "analog mode" and "motor state" are saved after a "read" command
static void test_gc_controller_identify_after_read()
{
  // Send a read origin command
  joybus_gcn_read_origin(&bus, response, spy, NULL);

  // Send a read command
  joybus_gcn_read(&bus, JOYBUS_GCN_ANALOG_MODE_3, JOYBUS_GCN_MOTOR_RUMBLE, response, spy, NULL);

  // Send an identify command
  joybus_identify(&bus, response, spy, NULL);

  // Verify the "analog_mode" and "motor_state" are present
  uint8_t expected_response[] = {0x09, 0x00, 0x0B};
  TEST_ASSERT_EQUAL(3, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_response, response, 3);
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();

  RUN_TEST(test_gc_controller_identify);
  RUN_TEST(test_gc_controller_identify_after_read_origin);
  RUN_TEST(test_gc_controller_identify_after_read);

  return UNITY_END();
}