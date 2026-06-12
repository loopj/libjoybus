#include <stddef.h>
#include <string.h>

#include <joybus/bus.h>
#include <joybus/checksum.h>
#include <joybus/commands.h>
#include <joybus/backend/loopback.h>
#include <joybus/host/common.h>
#include <joybus/host/n64.h>
#include <joybus/target/n64_controller.h>

#include "unity.h"

// Dummy pak backend
static void dummy_read_block(struct joybus_target_n64_pak *acc, uint16_t addr, uint8_t buf[JOYBUS_PAK_BLOCK_SIZE])
{
  // Return 32 zero bytes for all reads
  memset(buf, 0, JOYBUS_PAK_BLOCK_SIZE);
}

static void dummy_write_block(struct joybus_target_n64_pak *acc, uint16_t addr,
                              const uint8_t buf[JOYBUS_PAK_BLOCK_SIZE])
{
  // No-op
}

static const struct joybus_target_n64_pak_api dummy_pak_api = {
  .read_block  = dummy_read_block,
  .write_block = dummy_write_block,
};

// A loopback Joybus and an N64 controller target
struct joybus bus;
struct joybus_target_n64_controller controller;
struct joybus_target_n64_pak pak;

// Spy callback to capture responses
static uint8_t response[JOYBUS_BLOCK_SIZE];
static int response_len;
static void spy(struct joybus *bus, int result, void *user_data)
{
  response_len = result;
}

// Track reset callback invocations
static int reset_callback_count;
static void on_reset(struct joybus_target_n64_controller *controller)
{
  reset_callback_count++;
}

void setUp(void)
{
  // Initialize the loopback bus
  joybus_loopback_init(&bus);
  joybus_enable(&bus);

  // Initialize a standard N64 controller target — tests call joybus_target_register
  // to simulate "power on" at the moment of their choosing
  joybus_target_n64_controller_init(&controller);

  // Wire up the dummy pak backend
  pak.api = &dummy_pak_api;

  // Reset response capture
  response_len = -1;
  memset(response, 0, sizeof(response));

  // Reset reset-callback tracking
  reset_callback_count = 0;
}

void tearDown(void)
{
}

// Test that reset returns the controller ID and invokes the reset callback
static void test_n64_controller_reset()
{
  struct joybus_id id;

  // Register the reset callback before "power on"
  joybus_target_n64_controller_set_reset_cb(&controller, on_reset);

  // Power on (no pak)
  joybus_target_register(&bus, JOYBUS_TARGET(&controller));

  // Send a reset command
  joybus_reset(&bus, &id);

  // Expect the response to be the controller ID
  uint8_t expected[] = {0x05, 0x00, 0x02};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, &id, sizeof(id));

  // Expect the reset callback to have fired exactly once
  TEST_ASSERT_EQUAL(1, reset_callback_count);
}

// Test that the "identify" response is correct for a standard N64 controller with no pak
static void test_n64_controller_identify()
{
  struct joybus_id id;

  // Power on (no pak)
  joybus_target_register(&bus, JOYBUS_TARGET(&controller));

  // Send an identify command
  joybus_identify(&bus, &id);

  // Test device identify response is as expected
  uint8_t expected[] = {0x05, 0x00, 0x02};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, &id, sizeof(id));
}

// Test that the identify response reflects pak state when hotplugging an pak
static void test_n64_controller_pak_hotplug()
{
  struct joybus_id id;

  // Power on (no pak)
  joybus_target_register(&bus, JOYBUS_TARGET(&controller));

  // Send an identify command with no pak attached
  joybus_identify(&bus, &id);

  // Test identify reports pak absent
  uint8_t expected_absent[] = {0x05, 0x00, 0x02};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_absent, &id, sizeof(id));

  // Attach an pak
  joybus_target_n64_controller_attach_pak(&controller, &pak);

  // Send another identify command
  joybus_identify(&bus, &id);

  // Test identify reports pak changed
  uint8_t expected_changed[] = {0x05, 0x00, 0x03};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_changed, &id, sizeof(id));

  // Send another identify command
  joybus_identify(&bus, &id);

  // Test identify reports pak present on subsequent calls
  uint8_t expected_present[] = {0x05, 0x00, 0x01};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_present, &id, sizeof(id));
}

// Test that the identify response reflects pak state when unplugging an pak
static void test_n64_controller_pak_unplug()
{
  struct joybus_id id;

  // Attach an pak and then power on
  joybus_target_n64_controller_attach_pak(&controller, &pak);
  joybus_target_register(&bus, JOYBUS_TARGET(&controller));

  // Send an identify command
  joybus_identify(&bus, &id);

  // Test identify reports pak present
  uint8_t expected_present[] = {0x05, 0x00, 0x01};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_present, &id, sizeof(id));

  // Detach the pak
  joybus_target_n64_controller_detach_pak(&controller);

  // Send another identify command
  joybus_identify(&bus, &id);

  // Test identify reports pak absent
  uint8_t expected_absent[] = {0x05, 0x00, 0x02};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_absent, &id, sizeof(id));

  // Send another identify command
  joybus_identify(&bus, &id);

  // Test identify continues to report pak absent
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_absent, &id, sizeof(id));
}

// Test that the identify response reports "pak changed" when an pak is attached before the first identify
static void test_n64_controller_pak_hotplug_no_initial_identify()
{
  struct joybus_id id;

  // Power on (no pak), then hotplug before any identify is sent
  joybus_target_register(&bus, JOYBUS_TARGET(&controller));
  joybus_target_n64_controller_attach_pak(&controller, &pak);

  // Send an identify command
  joybus_identify(&bus, &id);

  // Test identify reports pak changed
  uint8_t expected_changed[] = {0x05, 0x00, 0x03};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_changed, &id, sizeof(id));

  // Send another identify command
  joybus_identify(&bus, &id);

  // Test identify reports pak present on subsequent calls
  uint8_t expected_present[] = {0x05, 0x00, 0x01};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_present, &id, sizeof(id));
}

// Test that the checksum error flag is set after a bad-checksum pak read, and cleared by a subsequent identify
static void test_n64_controller_bad_checksum_cleared_by_identify()
{
  struct joybus_id id;

  // Power on with pak attached, then bring up communication via identify
  joybus_target_n64_controller_attach_pak(&controller, &pak);
  joybus_target_register(&bus, JOYBUS_TARGET(&controller));
  joybus_identify(&bus, &id);

  // Send a raw pak read with a bad address checksum
  uint8_t bad_read_cmd[] = {JOYBUS_CMD_N64_PAK_READ, 0x80, 0x00};
  joybus_transfer(&bus, bad_read_cmd, sizeof(bad_read_cmd), response, JOYBUS_CMD_N64_PAK_READ_RX, spy, NULL);

  // Send an identify command
  joybus_identify(&bus, &id);

  // Test identify reports pak present with checksum error flag set
  uint8_t expected_error[] = {0x05, 0x00, 0x05};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_error, &id, sizeof(id));

  // Send another identify command
  joybus_identify(&bus, &id);

  // Test identify reports pak present with checksum error flag cleared
  uint8_t expected_present[] = {0x05, 0x00, 0x01};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_present, &id, sizeof(id));
}

// Test that the checksum error flag is cleared by a subsequent successful pak read
static void test_n64_controller_bad_checksum_cleared_by_successful_read()
{
  struct joybus_id id;

  // Power on with pak attached, then bring up communication via identify
  joybus_target_n64_controller_attach_pak(&controller, &pak);
  joybus_target_register(&bus, JOYBUS_TARGET(&controller));
  joybus_identify(&bus, &id);

  // Send a raw pak read with a bad address checksum
  uint8_t bad_read_cmd[] = {JOYBUS_CMD_N64_PAK_READ, 0x80, 0x00};
  joybus_transfer(&bus, bad_read_cmd, sizeof(bad_read_cmd), response, JOYBUS_CMD_N64_PAK_READ_RX, spy, NULL);

  // Send an pak read with a valid address checksum
  joybus_n64_pak_read_async(&bus, 0x8000, response, spy, NULL);

  // Send an identify command
  joybus_identify(&bus, &id);

  // Test identify reports pak present with no checksum error flag set
  uint8_t expected_present[] = {0x05, 0x00, 0x01};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_present, &id, 3);
}

// Test that pak_read with no pak attached returns 32 zeroes and the "pak absent" CRC
static void test_n64_controller_pak_read_no_pak()
{
  struct joybus_id id;

  // Power on (no pak)
  joybus_target_register(&bus, JOYBUS_TARGET(&controller));

  // Send an pak read with a valid address
  joybus_n64_pak_read_async(&bus, 0x8000, response, spy, NULL);

  // Expect 32 zero bytes followed by 0xFF
  uint8_t expected[JOYBUS_CMD_N64_PAK_READ_RX] = {0};
  expected[32]                                 = 0xFF;
  TEST_ASSERT_EQUAL(JOYBUS_CMD_N64_PAK_READ_RX, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response, JOYBUS_CMD_N64_PAK_READ_RX);
}

// Test that pak_write with no pak attached is accepted and returns the "pak absent" CRC
static void test_n64_controller_pak_write_no_pak()
{
  // Power on (no pak)
  joybus_target_register(&bus, JOYBUS_TARGET(&controller));

  // Send an pak write with a valid address and 32 bytes of 0x80
  uint8_t data[JOYBUS_PAK_BLOCK_SIZE];
  memset(data, 0x80, sizeof(data));
  joybus_n64_pak_write_async(&bus, 0x8000, data, response, spy, NULL);

  // Expect the response to be checksum ^ 0xFF (the "no pak" marker)
  uint8_t expected = joybus_data_checksum(data, sizeof(data)) ^ 0xFF;
  TEST_ASSERT_EQUAL(JOYBUS_CMD_N64_PAK_WRITE_RX, response_len);
  TEST_ASSERT_EQUAL_HEX8(expected, response[0]);
}

// Test that pak_read on a non-acked hotplugged pak behaves as if no pak is present
static void test_n64_controller_pak_read_while_changed()
{
  // Power on (no pak), then hotplug — status is now CHANGED, identify not yet called
  joybus_target_register(&bus, JOYBUS_TARGET(&controller));
  joybus_target_n64_controller_attach_pak(&controller, &pak);

  // Send an pak read with a valid address
  joybus_n64_pak_read_async(&bus, 0x8000, response, spy, NULL);

  // Expect the "no pak" response — 32 zero bytes followed by 0xFF
  uint8_t expected[JOYBUS_CMD_N64_PAK_READ_RX] = {0};
  expected[32]                                 = 0xFF;
  TEST_ASSERT_EQUAL(JOYBUS_CMD_N64_PAK_READ_RX, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response, JOYBUS_CMD_N64_PAK_READ_RX);
}

// Test that pak_write on a non-acked hotplugged pak behaves as if no pak is present
static void test_n64_controller_pak_write_while_changed()
{
  // Power on (no pak), then hotplug — status is now CHANGED, identify not yet called
  joybus_target_register(&bus, JOYBUS_TARGET(&controller));
  joybus_target_n64_controller_attach_pak(&controller, &pak);

  // Send an pak write with a valid address and 32 bytes of 0x80
  uint8_t data[JOYBUS_PAK_BLOCK_SIZE];
  memset(data, 0x80, sizeof(data));
  joybus_n64_pak_write_async(&bus, 0x8000, data, response, spy, NULL);

  // Expect the "no pak" response — checksum ^ 0xFF
  uint8_t expected = joybus_data_checksum(data, sizeof(data)) ^ 0xFF;
  TEST_ASSERT_EQUAL(JOYBUS_CMD_N64_PAK_WRITE_RX, response_len);
  TEST_ASSERT_EQUAL_HEX8(expected, response[0]);
}

// Test that a bad-checksum read with the pak present returns the "no pak" response
static void test_n64_controller_pak_read_bad_checksum()
{
  // Power on with pak attached (status = PRESENT, not CHANGED)
  joybus_target_n64_controller_attach_pak(&controller, &pak);
  joybus_target_register(&bus, JOYBUS_TARGET(&controller));

  // Send a raw pak read with a bad address checksum
  uint8_t bad_read_cmd[] = {JOYBUS_CMD_N64_PAK_READ, 0x80, 0x00};
  joybus_transfer(&bus, bad_read_cmd, sizeof(bad_read_cmd), response, JOYBUS_CMD_N64_PAK_READ_RX, spy, NULL);

  // Expect 32 zero bytes followed by 0xFF — the "no pak" CRC
  uint8_t expected[JOYBUS_CMD_N64_PAK_READ_RX] = {0};
  expected[32]                                 = 0xFF;
  TEST_ASSERT_EQUAL(JOYBUS_CMD_N64_PAK_READ_RX, response_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response, JOYBUS_CMD_N64_PAK_READ_RX);
}

// Test that a bad-checksum write with the pak present returns the "no pak" response
static void test_n64_controller_pak_write_bad_checksum()
{
  // Power on with pak attached (status = PRESENT, not CHANGED)
  joybus_target_n64_controller_attach_pak(&controller, &pak);
  joybus_target_register(&bus, JOYBUS_TARGET(&controller));

  // Build a raw pak write with a bad address checksum (low 5 bits zero, doesn't match)
  uint8_t bad_write_cmd[JOYBUS_CMD_N64_PAK_WRITE_TX];
  bad_write_cmd[0] = JOYBUS_CMD_N64_PAK_WRITE;
  bad_write_cmd[1] = 0x80;
  bad_write_cmd[2] = 0x00;
  memset(&bad_write_cmd[3], 0x80, JOYBUS_PAK_BLOCK_SIZE);

  joybus_transfer(&bus, bad_write_cmd, sizeof(bad_write_cmd), response, JOYBUS_CMD_N64_PAK_WRITE_RX, spy, NULL);

  // Expect checksum ^ 0xFF — the "no pak" marker
  uint8_t expected = joybus_data_checksum(&bad_write_cmd[3], JOYBUS_PAK_BLOCK_SIZE) ^ 0xFF;
  TEST_ASSERT_EQUAL(JOYBUS_CMD_N64_PAK_WRITE_RX, response_len);
  TEST_ASSERT_EQUAL_HEX8(expected, response[0]);
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();

  RUN_TEST(test_n64_controller_reset);
  RUN_TEST(test_n64_controller_identify);
  RUN_TEST(test_n64_controller_pak_hotplug);
  RUN_TEST(test_n64_controller_pak_unplug);
  RUN_TEST(test_n64_controller_pak_hotplug_no_initial_identify);
  RUN_TEST(test_n64_controller_bad_checksum_cleared_by_identify);
  RUN_TEST(test_n64_controller_bad_checksum_cleared_by_successful_read);
  RUN_TEST(test_n64_controller_pak_read_no_pak);
  RUN_TEST(test_n64_controller_pak_write_no_pak);
  RUN_TEST(test_n64_controller_pak_read_while_changed);
  RUN_TEST(test_n64_controller_pak_write_while_changed);
  RUN_TEST(test_n64_controller_pak_read_bad_checksum);
  RUN_TEST(test_n64_controller_pak_write_bad_checksum);

  return UNITY_END();
}
