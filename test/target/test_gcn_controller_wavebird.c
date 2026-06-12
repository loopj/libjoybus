#include <string.h>

#include "joybus/errors.h"
#include <joybus/bus.h>
#include <joybus/identify.h>
#include <joybus/backend/loopback.h>
#include <joybus/host/common.h>
#include <joybus/host/gcn.h>
#include <joybus/target/gcn_controller.h>

#include "unity.h"

// A loopback Joybus and a WaveBird controller target
struct joybus bus;
struct joybus_target_gcn_controller controller;

void setUp(void)
{
  // Initialize the loopback bus
  joybus_loopback_init(&bus);
  joybus_enable(&bus);

  // Register a WaveBird receiver controller target
  joybus_target_gcn_controller_init_wavebird(&controller);
  joybus_target_register(&bus, JOYBUS_TARGET(&controller));
}

void tearDown(void)
{
}

// Test that the initial identify response is correct for a WaveBird receiver
static void test_wavebird_identify()
{
  struct joybus_id id;

  // Send an identify command
  joybus_identify(&bus, &id);

  // Test identify response is as expected
  uint8_t expected_response[] = {0xA8, 0x00, 0x00};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_response, &id, sizeof(id));
}

// Test that the identify response is correct when setting the wireless ID
static void test_wavebird_identify_after_set_wireless_id()
{
  struct joybus_id id;

  // Set a 10-bit wireless ID
  joybus_target_gcn_controller_set_wireless_id(&controller, 0x2B1);
  TEST_ASSERT_EQUAL(0x2B1, joybus_target_gcn_controller_get_wireless_id(&controller));

  // Send an identify command
  joybus_identify(&bus, &id);

  // Test device identify response includes the controller ID
  uint8_t expected_response[] = {0xE9, 0x80, 0xB1};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_response, &id, sizeof(id));
}

// Test that the identify response is correct when setting the wireless ID multiple times
static void test_wavebird_identify_after_set_wireless_id_multiple()
{
  struct joybus_id id;

  // Set a 10-bit wireless ID
  joybus_target_gcn_controller_set_wireless_id(&controller, 0x2B1);
  TEST_ASSERT_EQUAL(0x2B1, joybus_target_gcn_controller_get_wireless_id(&controller));

  // Set a different 10-bit wireless ID
  joybus_target_gcn_controller_set_wireless_id(&controller, 0x32F);
  TEST_ASSERT_EQUAL(0x32F, joybus_target_gcn_controller_get_wireless_id(&controller));

  // Send an identify command
  joybus_identify(&bus, &id);

  // Test device identify response includes the most recent controller ID
  uint8_t expected_response[] = {0xE9, 0xC0, 0x2F};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_response, &id, sizeof(id));
}

// Test that the device identify response is correct when fixing the wireless ID
static void test_wavebird_identify_after_fix_device()
{
  struct joybus_id id;

  // Send an identify command
  joybus_identify(&bus, &id);

  // Test identify response is as expected
  uint8_t expected_identify_response[] = {0xA8, 0x00, 0x00};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_identify_response, &id, sizeof(id));

  // Set the wireless ID (e.g. after packet reception)
  joybus_target_gcn_controller_set_wireless_id(&controller, 0x2B1);

  // Send an identify command
  joybus_identify(&bus, &id);

  // Test identify response is as expected
  uint8_t expected_identify_response_2[] = {0xE9, 0x80, 0xB1};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_identify_response_2, &id, sizeof(id));

  // Send a fix device command
  joybus_gcn_fix_device(&bus, 0x2B1, &id);

  // Check fix device response is as expected
  uint8_t expected_fix_response[] = {0xEB, 0x90, 0xB1};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_fix_response, &id, sizeof(id));

  // Send an identify command
  joybus_identify(&bus, &id);

  // Test identify response includes the fixed controller ID
  uint8_t expected_response[] = {0xEB, 0x90, 0xB1};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_response, &id, sizeof(id));
}

// Test that the identify response is correct when the console fixes the wireless ID,
// but we have not yet received a packet from the controller
static void test_wavebird_fix_device_without_wireless_id()
{
  struct joybus_id id;

  // Send an identify command
  joybus_identify(&bus, &id);

  // Test device identify response is as expected
  uint8_t expected_identify_response[] = {0xA8, 0x00, 0x00};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_identify_response, &id, sizeof(id));

  // Send a fix device command
  joybus_gcn_fix_device(&bus, 0x2B1, &id);

  // Check fix device response is as expected
  uint8_t expected_fix_response[] = {0xAB, 0x90, 0xB1};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_fix_response, &id, sizeof(id));

  // Send an identify command
  joybus_identify(&bus, &id);

  // Test identify response includes the fixed controller ID
  uint8_t expected_identify_response_2[] = {0xAB, 0x90, 0xB1};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_identify_response_2, &id, sizeof(id));
}

// Test that setting wireless ID fails when the controller ID has already been fixed
static void test_wavebird_set_wireless_id_when_fixed(void)
{
  struct joybus_id id;

  // Set a wireless ID
  joybus_target_gcn_controller_set_wireless_id(&controller, 0x2B1);

  // Send a fix device command
  joybus_gcn_fix_device(&bus, 0x2B1, &id);

  // Try to set a different 10-bit wireless ID
  joybus_target_gcn_controller_set_wireless_id(&controller, 0x123);

  // Check the wireless ID has not changed
  TEST_ASSERT_EQUAL_HEX16(0x2B1, joybus_target_gcn_controller_get_wireless_id(&controller));
}

static void test_wavebird_set_origin(void)
{
  struct joybus_id id;

  // Set a 10-bit wireless ID
  joybus_target_gcn_controller_set_wireless_id(&controller, 0x2B1);

  // Send an identify command
  joybus_identify(&bus, &id);

  // Test device identify response is as expected
  uint8_t expected_identify_response[] = {0xE9, 0x80, 0xB1};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_identify_response, &id, sizeof(id));

  // Send a fix device command
  joybus_gcn_fix_device(&bus, 0x2B1, &id);

  // Send an identify command
  joybus_identify(&bus, &id);

  // Test device identify response is as expected
  uint8_t expected_identify_response_2[] = {0xEB, 0x90, 0xB1};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_identify_response_2, &id, sizeof(id));

  // Set the wireless origin
  struct joybus_gcn_controller_state origin = {
    .stick_x       = 0x85,
    .stick_y       = 0x86,
    .substick_x    = 0x87,
    .substick_y    = 0x88,
    .trigger_left  = 0x11,
    .trigger_right = 0x12,
  };
  joybus_target_gcn_controller_set_origin(&controller, &origin);

  // Check the origin state is set correctly
  TEST_ASSERT_EQUAL_UINT8(0x85, controller.origin.stick_x);
  TEST_ASSERT_EQUAL_UINT8(0x86, controller.origin.stick_y);
  TEST_ASSERT_EQUAL_UINT8(0x87, controller.origin.substick_x);
  TEST_ASSERT_EQUAL_UINT8(0x88, controller.origin.substick_y);
  TEST_ASSERT_EQUAL_UINT8(0x11, controller.origin.trigger_left);
  TEST_ASSERT_EQUAL_UINT8(0x12, controller.origin.trigger_right);

  // Send an identify command
  joybus_identify(&bus, &id);

  // Test device identify response includes the origin flag
  uint8_t expected_identify_response_3[] = {0xEB, 0xB0, 0xB1};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_identify_response_3, &id, sizeof(id));
}

// Test that the probe device response is correct, and ignored after setting the wireless ID
static void test_wavebird_probe_response()
{
  uint8_t probe_response[JOYBUS_CMD_GCN_PROBE_DEVICE_RX];

  // Send a probe command
  joybus_gcn_probe_device(&bus, probe_response);

  // Test probe response is as expected
  uint8_t expected_probe_response[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_probe_response, probe_response, sizeof(probe_response));

  // Set a 10-bit wireless ID
  joybus_target_gcn_controller_set_wireless_id(&controller, 0x2B1);
  TEST_ASSERT_EQUAL(0x2B1, joybus_target_gcn_controller_get_wireless_id(&controller));

  // Send another probe command
  int rc = joybus_gcn_probe_device(&bus, probe_response);

  // Test probe response is ignored after setting wireless ID
  TEST_ASSERT_EQUAL(-JOYBUS_ERR_TIMEOUT, rc);
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
