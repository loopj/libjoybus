#include <string.h>

#include <joybus/bus.h>
#include <joybus/commands.h>
#include <joybus/backend/loopback.h>
#include <joybus/host/common.h>
#include <joybus/host/gamecube.h>
#include <joybus/target/gc_controller.h>

#include "unity.h"

// A loopback Joybus and a WaveBird controller target
struct joybus bus;
struct joybus_gc_controller controller;

// Spy callback to capture responses
static uint8_t response[JOYBUS_BLOCK_SIZE];
static int response_len = -1;
static void spy(struct joybus *bus, int result, void *user_data)
{
  response_len = result;
}

void setUp(void)
{
  // Initialize the loopback bus
  joybus_loopback_init(&bus);
  joybus_enable(&bus);

  // Register a WaveBird receiver controller target
  joybus_gc_controller_init(&controller, JOYBUS_WAVEBIRD_RECEIVER);
  joybus_target_register(&bus, JOYBUS_TARGET(&controller));
}

void tearDown(void)
{
}

// Test that the initial identify response is correct for a WaveBird receiver
static void test_wavebird_identify()
{
  // Send an identify command
  joybus_identify(&bus, response, spy, NULL);

  // Test identify response is as expected
  uint8_t expected_response[] = {0xA8, 0x00, 0x00};
  TEST_ASSERT_EQUAL(3, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_response, response, 3);
}

// Test that the identify response is correct when setting the wireless ID
static void test_wavebird_identify_after_set_wireless_id()
{
  // Set a 10-bit wireless ID
  joybus_gc_controller_set_wireless_id(&controller, 0x2B1);
  TEST_ASSERT_EQUAL(0x2B1, joybus_gc_controller_get_wireless_id(&controller));

  // Send an identify command
  joybus_identify(&bus, response, spy, NULL);

  // Test device identify response includes the controller ID
  uint8_t expected_response[] = {0xE9, 0x80, 0xB1};
  TEST_ASSERT_EQUAL(3, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_response, response, 3);
}

// Test that the identify response is correct when setting the wireless ID multiple times
static void test_wavebird_identify_after_set_wireless_id_multiple()
{
  // Set a 10-bit wireless ID
  joybus_gc_controller_set_wireless_id(&controller, 0x2B1);
  TEST_ASSERT_EQUAL(0x2B1, joybus_gc_controller_get_wireless_id(&controller));

  // Set a different 10-bit wireless ID
  joybus_gc_controller_set_wireless_id(&controller, 0x32F);
  TEST_ASSERT_EQUAL(0x32F, joybus_gc_controller_get_wireless_id(&controller));

  // Send an identify command
  joybus_identify(&bus, response, spy, NULL);

  // Test device identify response includes the most recent controller ID
  uint8_t expected_response[] = {0xE9, 0xC0, 0x2F};
  TEST_ASSERT_EQUAL(3, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_response, response, 3);
}

// Test that the device identify response is correct when fixing the wireless ID
static void test_wavebird_identify_after_fix_device()
{
  // Send an identify command
  joybus_identify(&bus, response, spy, NULL);

  // Test identify response is as expected
  uint8_t expected_identify_response[] = {0xA8, 0x00, 0x00};
  TEST_ASSERT_EQUAL(3, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_identify_response, response, 3);

  // Set the wireless ID (e.g. after packet reception)
  joybus_gc_controller_set_wireless_id(&controller, 0x2B1);

  // Send an identify command
  joybus_identify(&bus, response, spy, NULL);

  // Test identify response is as expected
  uint8_t expected_identify_response_2[] = {0xE9, 0x80, 0xB1};
  TEST_ASSERT_EQUAL(3, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_identify_response_2, response, 3);

  // Send a fix device command
  joybus_gcn_fix_device(&bus, 0x2B1, response, spy, NULL);

  // Check fix device response is as expected
  uint8_t expected_fix_response[] = {0xEB, 0x90, 0xB1};
  TEST_ASSERT_EQUAL(3, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_fix_response, response, 3);

  // Send an identify command
  joybus_identify(&bus, response, spy, NULL);

  // Test identify response includes the fixed controller ID
  uint8_t expected_response[] = {0xEB, 0x90, 0xB1};
  TEST_ASSERT_EQUAL(3, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_response, response, 3);
}

// Test that the identify response is correct when the console fixes the wireless ID,
// but we have not yet received a packet from the controller
static void test_wavebird_fix_device_without_wireless_id()
{
  // Send an identify command
  joybus_identify(&bus, response, spy, NULL);

  // Test device identify response is as expected
  uint8_t expected_identify_response[] = {0xA8, 0x00, 0x00};
  TEST_ASSERT_EQUAL(3, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_identify_response, response, 3);

  // Send a fix device command
  joybus_gcn_fix_device(&bus, 0x2B1, response, spy, NULL);

  // Check fix device response is as expected
  uint8_t expected_fix_response[] = {0xAB, 0x90, 0xB1};
  TEST_ASSERT_EQUAL(3, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_fix_response, response, 3);

  // Send an identify command
  joybus_identify(&bus, response, spy, NULL);

  // Test identify response includes the fixed controller ID
  uint8_t expected_identify_response_2[] = {0xAB, 0x90, 0xB1};
  TEST_ASSERT_EQUAL(3, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_identify_response_2, response, 3);
}

// Test that setting wireless ID fails when the controller ID has already been fixed
static void test_wavebird_set_wireless_id_when_fixed(void)
{
  // Set a wireless ID
  joybus_gc_controller_set_wireless_id(&controller, 0x2B1);

  // Send a fix device command
  joybus_gcn_fix_device(&bus, 0x2B1, response, spy, NULL);

  // Try to set a different 10-bit wireless ID
  joybus_gc_controller_set_wireless_id(&controller, 0x123);

  // Check the wireless ID has not changed
  TEST_ASSERT_EQUAL_HEX16(0x2B1, joybus_gc_controller_get_wireless_id(&controller));
}

static void test_wavebird_set_origin(void)
{
  // Set a 10-bit wireless ID
  joybus_gc_controller_set_wireless_id(&controller, 0x2B1);

  // Send an identify command
  joybus_identify(&bus, response, spy, NULL);

  // Test device identify response is as expected
  uint8_t expected_identify_response[] = {0xE9, 0x80, 0xB1};
  TEST_ASSERT_EQUAL(3, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_identify_response, response, 3);

  // Send a fix device command
  joybus_gcn_fix_device(&bus, 0x2B1, response, spy, NULL);

  // Send an identify command
  joybus_identify(&bus, response, spy, NULL);

  // Test device identify response is as expected
  uint8_t expected_identify_response_2[] = {0xEB, 0x90, 0xB1};
  TEST_ASSERT_EQUAL(3, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_identify_response_2, response, 3);

  // Set the wireless origin
  struct joybus_gc_controller_input origin = {
    .stick_x       = 0x85,
    .stick_y       = 0x86,
    .substick_x    = 0x87,
    .substick_y    = 0x88,
    .trigger_left  = 0x11,
    .trigger_right = 0x12,
  };
  joybus_gc_controller_set_origin(&controller, &origin);

  // Check the origin state is set correctly
  TEST_ASSERT_EQUAL_UINT8(0x85, controller.origin.stick_x);
  TEST_ASSERT_EQUAL_UINT8(0x86, controller.origin.stick_y);
  TEST_ASSERT_EQUAL_UINT8(0x87, controller.origin.substick_x);
  TEST_ASSERT_EQUAL_UINT8(0x88, controller.origin.substick_y);
  TEST_ASSERT_EQUAL_UINT8(0x11, controller.origin.trigger_left);
  TEST_ASSERT_EQUAL_UINT8(0x12, controller.origin.trigger_right);

  // Send an identify command
  joybus_identify(&bus, response, spy, NULL);

  // Test device identify response includes the origin flag
  uint8_t expected_identify_response_3[] = {0xEB, 0xB0, 0xB1};
  TEST_ASSERT_EQUAL(3, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_identify_response_3, response, 3);
}

// Test that the probe device response is correct, and ignored after setting the wireless ID
static void test_wavebird_probe_response()
{
  // Send a probe command
  joybus_gcn_probe_device(&bus, response, spy, NULL);
  // Test probe response is as expected
  uint8_t expected_probe_response[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  TEST_ASSERT_EQUAL(8, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_probe_response, response, 8);

  // Set a 10-bit wireless ID
  joybus_gc_controller_set_wireless_id(&controller, 0x2B1);
  TEST_ASSERT_EQUAL(0x2B1, joybus_gc_controller_get_wireless_id(&controller));

  // Send another probe command
  joybus_gcn_probe_device(&bus, response, spy, NULL);

  // Test probe response is ignored after setting wireless ID
  TEST_ASSERT_EQUAL(0, response_len);
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();

  RUN_TEST(test_wavebird_identify);
  RUN_TEST(test_wavebird_identify_after_set_wireless_id);
  RUN_TEST(test_wavebird_identify_after_set_wireless_id_multiple);
  RUN_TEST(test_wavebird_identify_after_fix_device);
  RUN_TEST(test_wavebird_fix_device_without_wireless_id);
  RUN_TEST(test_wavebird_set_wireless_id_when_fixed);
  RUN_TEST(test_wavebird_set_origin);
  RUN_TEST(test_wavebird_probe_response);

  return UNITY_END();
}