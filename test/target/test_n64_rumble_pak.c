#include <string.h>

#include <joybus/bus.h>
#include <joybus/checksum.h>
#include <joybus/commands.h>
#include <joybus/target.h>
#include <joybus/target/n64_controller.h>
#include <joybus/target/n64_pak.h>
#include <joybus/target/n64_rumble_pak.h>

#include "unity.h"

#include "harness.h"

// The rumble pak under test, and a controller to host it for the wire tests
static struct joybus_target_n64_rumble_pak rumble;
static struct joybus_target_n64_controller controller;

// Spy for the motor state change callback
static int motor_count;
static bool motor_last_state;
static void on_motor_change(struct joybus_target_n64_rumble_pak *pak, bool active)
{
  motor_count++;
  motor_last_state = active;
}

// ---------------------------------------------------------------------------
// Direct pak-API helpers
// ---------------------------------------------------------------------------

// Read a block directly through the pak API
static void pak_read(uint16_t addr, uint8_t buf[JOYBUS_PAK_BLOCK_SIZE])
{
  rumble.base.api->read_block(&rumble.base, addr, buf);
}

// Write a block directly through the pak API
static void pak_write(uint16_t addr, const uint8_t buf[JOYBUS_PAK_BLOCK_SIZE])
{
  rumble.base.api->write_block(&rumble.base, addr, buf);
}

// ---------------------------------------------------------------------------
// Wire-level helpers
// ---------------------------------------------------------------------------

// Build a wire address: an aligned block address with its checksum in the low 5 bits
static uint16_t valid_pak_addr(uint16_t block_addr)
{
  return block_addr | joybus_address_checksum(block_addr >> 5);
}

// Send a pak read command for a block address through the controller
static void wire_pak_read(uint16_t block_addr)
{
  uint16_t addr     = valid_pak_addr(block_addr);
  uint8_t command[] = {JOYBUS_CMD_N64_PAK_READ, addr >> 8, addr & 0xFF};
  send_command(command, sizeof(command));
}

// Send a pak write command of 32 x `fill` to a block address through the controller
static void wire_pak_write(uint16_t block_addr, uint8_t fill)
{
  uint16_t addr = valid_pak_addr(block_addr);
  uint8_t command[JOYBUS_CMD_N64_PAK_WRITE_TX];
  command[0] = JOYBUS_CMD_N64_PAK_WRITE;
  command[1] = addr >> 8;
  command[2] = addr & 0xFF;
  memset(&command[3], fill, JOYBUS_PAK_BLOCK_SIZE);
  send_command(command, sizeof(command));
}

void setUp(void)
{
  // Recreate the pak from scratch and wire up the motor spy
  joybus_target_n64_rumble_pak_init(&rumble);
  joybus_target_n64_rumble_pak_set_motor_cb(&rumble, on_motor_change);

  // Host the pak in a controller for the wire-level tests; attaching before
  // registration reports it present with no change flag to acknowledge
  joybus_target_n64_controller_init(&controller);
  joybus_target_n64_controller_attach_pak(&controller, JOYBUS_TARGET_N64_PAK(&rumble));

  // Point the harness at the controller and clear recorded responses
  harness_reset(JOYBUS_TARGET(&controller));

  // Reset the motor spy
  motor_count      = 0;
  motor_last_state = false;
}

void tearDown(void)
{
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

// Test the initial state set up by init
static void test_init_defaults(void)
{
  TEST_ASSERT_FALSE(rumble.active);
  TEST_ASSERT_NOT_NULL(rumble.base.api->read_block);
  TEST_ASSERT_NOT_NULL(rumble.base.api->write_block);
}

// ---------------------------------------------------------------------------
// Probe region / signature enable
// ---------------------------------------------------------------------------

// Test that a fresh pak is disabled: the probe region reads zeros until the host writes the 0x80 signature
static void test_probe_disabled_by_default(void)
{
  uint8_t buf[JOYBUS_PAK_BLOCK_SIZE];
  uint8_t zeros[JOYBUS_PAK_BLOCK_SIZE] = {0};

  pak_read(0x8000, buf);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(zeros, buf, sizeof(buf));
}

// Test that writing the 0x80 signature enables the probe region, which then reads back 0x80 x 32
static void test_probe_enable_with_signature(void)
{
  uint8_t buf[JOYBUS_PAK_BLOCK_SIZE];
  uint8_t signature[JOYBUS_PAK_BLOCK_SIZE];
  memset(signature, 0x80, sizeof(signature));

  memset(buf, 0x80, sizeof(buf));
  pak_write(0x8000, buf);

  pak_read(0x8000, buf);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(signature, buf, sizeof(buf));
}

// Test that the enable register latches the LAST byte of the write: the signature in the last byte enables it, the
// signature in any other byte does not
static void test_probe_enable_latches_last_byte(void)
{
  uint8_t buf[JOYBUS_PAK_BLOCK_SIZE];
  uint8_t signature[JOYBUS_PAK_BLOCK_SIZE];
  uint8_t zeros[JOYBUS_PAK_BLOCK_SIZE] = {0};
  memset(signature, 0x80, sizeof(signature));

  // 0x80 only in the last byte -> enabled
  memset(buf, 0x00, sizeof(buf));
  buf[JOYBUS_PAK_BLOCK_SIZE - 1] = 0x80;
  pak_write(0x8000, buf);
  pak_read(0x8000, buf);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(signature, buf, sizeof(buf));

  // 0x80 everywhere except the last byte -> disabled
  memset(buf, 0x80, sizeof(buf));
  buf[JOYBUS_PAK_BLOCK_SIZE - 1] = 0x00;
  pak_write(0x8000, buf);
  pak_read(0x8000, buf);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(zeros, buf, sizeof(buf));
}

// Test that only an exact 0x80 last byte enables the signature: nearby values with bit 7 set, the low bit set, or all
// bits set do not
static void test_probe_enable_requires_exact_signature(void)
{
  uint8_t buf[JOYBUS_PAK_BLOCK_SIZE];
  uint8_t zeros[JOYBUS_PAK_BLOCK_SIZE] = {0};
  const uint8_t non_signatures[]       = {0x81, 0xC0, 0x40, 0xFF, 0x01, 0x7F, 0x08, 0x00};

  for (size_t i = 0; i < sizeof(non_signatures); i++) {
    // Enable, then a non-signature last byte must leave it disabled
    memset(buf, 0x80, sizeof(buf));
    pak_write(0x8000, buf);
    memset(buf, non_signatures[i], sizeof(buf));
    pak_write(0x8000, buf);

    pak_read(0x8000, buf);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(zeros, buf, sizeof(buf));
  }
}

// Test the probe region extent [0x8000, 0xC000): the block below reads as SRAM zeros, the whole region reads the
// signature, and the motor region reads zeros
static void test_probe_region_extent(void)
{
  uint8_t buf[JOYBUS_PAK_BLOCK_SIZE];
  uint8_t signature[JOYBUS_PAK_BLOCK_SIZE];
  uint8_t zeros[JOYBUS_PAK_BLOCK_SIZE] = {0};
  memset(signature, 0x80, sizeof(signature));

  memset(buf, 0x80, sizeof(buf));
  pak_write(0x8000, buf);

  // Last block before the region is SRAM space
  pak_read(0x7FE0, buf);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(zeros, buf, sizeof(buf));

  // Region spans 0x8000 through the last block 0xBFE0
  pak_read(0x8000, buf);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(signature, buf, sizeof(buf));
  pak_read(0x9FE0, buf);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(signature, buf, sizeof(buf));
  pak_read(0xA000, buf);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(signature, buf, sizeof(buf));
  pak_read(0xBFE0, buf);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(signature, buf, sizeof(buf));

  // First block of the motor region reads zeros
  pak_read(0xC000, buf);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(zeros, buf, sizeof(buf));
}

// Test that the probe region is one shared register, not per-block storage: enabling via one block makes a different
// block read the signature, and disabling via a third block clears the whole region
static void test_probe_is_single_register(void)
{
  uint8_t buf[JOYBUS_PAK_BLOCK_SIZE];
  uint8_t signature[JOYBUS_PAK_BLOCK_SIZE];
  uint8_t zeros[JOYBUS_PAK_BLOCK_SIZE] = {0};
  memset(signature, 0x80, sizeof(signature));

  // Enable via 0x8000, observe at 0xA000
  memset(buf, 0x80, sizeof(buf));
  pak_write(0x8000, buf);
  pak_read(0xA000, buf);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(signature, buf, sizeof(buf));

  // Disable via 0xB000, observe at 0x8000
  memset(buf, 0x00, sizeof(buf));
  pak_write(0xB000, buf);
  pak_read(0x8000, buf);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(zeros, buf, sizeof(buf));
}

// Test that writing to the SRAM space is ignored: it does not enable the signature or change the motor state
static void test_write_sram_ignored(void)
{
  uint8_t buf[JOYBUS_PAK_BLOCK_SIZE];
  uint8_t signature[JOYBUS_PAK_BLOCK_SIZE];
  memset(signature, 0x80, sizeof(signature));

  memset(buf, 0x80, sizeof(buf));
  pak_write(0x8000, buf);
  memset(buf, 0x01, sizeof(buf));
  pak_write(0x0000, buf);

  TEST_ASSERT_FALSE(rumble.active);
  TEST_ASSERT_EQUAL(0, motor_count);

  pak_read(0x8000, buf);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(signature, buf, sizeof(buf));
}

// ---------------------------------------------------------------------------
// Motor control
// ---------------------------------------------------------------------------

// Test that the motor does not run while the signature is disabled, even for a
// canonical motor-start write
static void test_motor_requires_signature_enabled(void)
{
  uint8_t buf[JOYBUS_PAK_BLOCK_SIZE];

  memset(buf, 0x01, sizeof(buf));
  pak_write(0xC000, buf);

  TEST_ASSERT_FALSE(rumble.active);
  TEST_ASSERT_EQUAL(0, motor_count);
}

// Test that, once enabled, a motor-start write activates the motor and fires
// the callback
static void test_motor_start_fires_callback(void)
{
  uint8_t buf[JOYBUS_PAK_BLOCK_SIZE];

  memset(buf, 0x80, sizeof(buf));
  pak_write(0x8000, buf);
  memset(buf, 0x01, sizeof(buf));
  pak_write(0xC000, buf);

  TEST_ASSERT_TRUE(rumble.active);
  TEST_ASSERT_EQUAL(1, motor_count);
  TEST_ASSERT_TRUE(motor_last_state);
}

// Test that a motor stop write after a start deactivates the motor
static void test_motor_stop_fires_callback(void)
{
  uint8_t buf[JOYBUS_PAK_BLOCK_SIZE];

  memset(buf, 0x80, sizeof(buf));
  pak_write(0x8000, buf);
  memset(buf, 0x01, sizeof(buf));
  pak_write(0xC000, buf);
  memset(buf, 0x00, sizeof(buf));
  pak_write(0xC000, buf);

  TEST_ASSERT_FALSE(rumble.active);
  TEST_ASSERT_EQUAL(2, motor_count);
  TEST_ASSERT_FALSE(motor_last_state);
}

// Test that the motor follows the LOW BIT of the last byte: odd values run it, even values stop it (0x02 / 0xFE are
// off, unlike a naive "any nonzero" rule)
static void test_motor_follows_low_bit_of_last_byte(void)
{
  uint8_t buf[JOYBUS_PAK_BLOCK_SIZE];

  memset(buf, 0x80, sizeof(buf));
  pak_write(0x8000, buf);

  memset(buf, 0x01, sizeof(buf)); // odd
  pak_write(0xC000, buf);
  TEST_ASSERT_TRUE(rumble.active);

  memset(buf, 0x02, sizeof(buf)); // even
  pak_write(0xC000, buf);
  TEST_ASSERT_FALSE(rumble.active);

  memset(buf, 0x03, sizeof(buf)); // odd
  pak_write(0xC000, buf);
  TEST_ASSERT_TRUE(rumble.active);

  memset(buf, 0xFE, sizeof(buf)); // even
  pak_write(0xC000, buf);
  TEST_ASSERT_FALSE(rumble.active);

  memset(buf, 0xFF, sizeof(buf)); // odd
  pak_write(0xC000, buf);
  TEST_ASSERT_TRUE(rumble.active);
}

// Test that the motor keys off the LAST byte, not the first: an odd last byte runs it regardless of the first byte, and
// an even last byte stops it
static void test_motor_uses_last_byte_not_first(void)
{
  uint8_t buf[JOYBUS_PAK_BLOCK_SIZE];

  memset(buf, 0x80, sizeof(buf));
  pak_write(0x8000, buf);

  // First byte even, last byte odd -> motor on
  memset(buf, 0x00, sizeof(buf));
  buf[JOYBUS_PAK_BLOCK_SIZE - 1] = 0x01;
  pak_write(0xC000, buf);
  TEST_ASSERT_TRUE(rumble.active);

  // First byte odd, last byte even -> motor off
  memset(buf, 0x01, sizeof(buf));
  buf[JOYBUS_PAK_BLOCK_SIZE - 1] = 0x00;
  pak_write(0xC000, buf);
  TEST_ASSERT_FALSE(rumble.active);
}

// Test that the callback fires only when the motor state changes
static void test_motor_callback_edge_triggered(void)
{
  uint8_t buf[JOYBUS_PAK_BLOCK_SIZE];

  memset(buf, 0x80, sizeof(buf));
  pak_write(0x8000, buf);

  memset(buf, 0x01, sizeof(buf));
  pak_write(0xC000, buf);
  pak_write(0xC000, buf);
  pak_write(0xC000, buf);
  TEST_ASSERT_EQUAL(1, motor_count);

  memset(buf, 0x00, sizeof(buf));
  pak_write(0xC000, buf);
  pak_write(0xC000, buf);
  TEST_ASSERT_EQUAL(2, motor_count);
}

// Test that a stop write with the motor already off does nothing
static void test_motor_stop_when_already_off(void)
{
  uint8_t buf[JOYBUS_PAK_BLOCK_SIZE];

  memset(buf, 0x80, sizeof(buf));
  pak_write(0x8000, buf);
  memset(buf, 0x00, sizeof(buf));
  pak_write(0xC000, buf);

  TEST_ASSERT_FALSE(rumble.active);
  TEST_ASSERT_EQUAL(0, motor_count);
}

// Test the motor region extent [0xC000, 0x10000): the first block, an interior block, and the last block (top of the
// address space) all drive the motor
static void test_motor_region_extent(void)
{
  uint8_t buf[JOYBUS_PAK_BLOCK_SIZE];

  memset(buf, 0x80, sizeof(buf));
  pak_write(0x8000, buf);

  memset(buf, 0x01, sizeof(buf));
  pak_write(0xC000, buf);
  TEST_ASSERT_TRUE(rumble.active);
  memset(buf, 0x00, sizeof(buf));
  pak_write(0xC000, buf);

  memset(buf, 0x01, sizeof(buf));
  pak_write(0xE000, buf);
  TEST_ASSERT_TRUE(rumble.active);
  memset(buf, 0x00, sizeof(buf));
  pak_write(0xE000, buf);

  memset(buf, 0x01, sizeof(buf));
  pak_write(0xFFE0, buf);
  TEST_ASSERT_TRUE(rumble.active);
}

// Test that motor state still updates when no callback is registered
static void test_motor_write_without_callback(void)
{
  uint8_t buf[JOYBUS_PAK_BLOCK_SIZE];

  joybus_target_n64_rumble_pak_set_motor_cb(&rumble, NULL);

  memset(buf, 0x80, sizeof(buf));
  pak_write(0x8000, buf);
  memset(buf, 0x01, sizeof(buf));
  pak_write(0xC000, buf);

  TEST_ASSERT_TRUE(rumble.active);
  TEST_ASSERT_EQUAL(0, motor_count);
}

// ---------------------------------------------------------------------------
// Wire-level sequences through a controller
// ---------------------------------------------------------------------------

// Test that a probe read before the signature is enabled returns 32 zeros with a valid data checksum
static void test_wire_probe_read_before_enable_returns_zeros(void)
{
  wire_pak_read(0x8000);

  uint8_t expected[JOYBUS_CMD_N64_PAK_READ_RX] = {0};
  expected[JOYBUS_PAK_BLOCK_SIZE]              = joybus_data_checksum(expected, JOYBUS_PAK_BLOCK_SIZE);

  TEST_ASSERT_EQUAL(JOYBUS_CMD_N64_PAK_READ_RX, response.len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));
}

// Test a typical "motor init" sequence over the wire: a probe write of 0x80
// enables the signature, after which the probe read returns 0x80 x 32 + CRC
static void test_wire_probe_enable_then_read_returns_signature(void)
{
  // Enable the rumble pak
  wire_pak_write(0x8000, 0x80);

  // Read back the signature
  wire_pak_read(0x8000);

  // Verify the signature matches what we expect
  uint8_t expected[JOYBUS_CMD_N64_PAK_READ_RX];
  memset(expected, 0x80, JOYBUS_PAK_BLOCK_SIZE);
  expected[JOYBUS_PAK_BLOCK_SIZE] = joybus_data_checksum(expected, JOYBUS_PAK_BLOCK_SIZE);

  TEST_ASSERT_EQUAL(JOYBUS_CMD_N64_PAK_READ_RX, response.len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));
}

// Test the motor sequence a console performs after enabling: pak writes of 32 x 0x01 / 0x00 toggle the motor, and each
// echoed CRC matches the one the console computes over the payload it sent
static void test_wire_motor_start_stop(void)
{
  // Enable the rumble pak
  wire_pak_write(0x8000, 0x80);

  // Start the motor
  wire_pak_write(0xC000, 0x01);

  // Verify the motor starts, the response matches what we expect, and the motor state is updated
  TEST_ASSERT_EQUAL(JOYBUS_CMD_N64_PAK_WRITE_RX, response.len);
  TEST_ASSERT_EQUAL_HEX8(0xEB, response.data[0]);
  TEST_ASSERT_TRUE(rumble.active);
  TEST_ASSERT_EQUAL(1, motor_count);
  TEST_ASSERT_TRUE(motor_last_state);

  // Stop the motor
  wire_pak_write(0xC000, 0x00);

  // Verify the motor stops, the response matches what we expect, and the motor state is updated
  TEST_ASSERT_EQUAL_HEX8(0x00, response.data[0]);
  TEST_ASSERT_FALSE(rumble.active);
  TEST_ASSERT_EQUAL(2, motor_count);
  TEST_ASSERT_FALSE(motor_last_state);
}

int main(void)
{
  UNITY_BEGIN();

  // Init
  RUN_TEST(test_init_defaults);

  // Probe region / signature enable
  RUN_TEST(test_probe_disabled_by_default);
  RUN_TEST(test_probe_enable_with_signature);
  RUN_TEST(test_probe_enable_latches_last_byte);
  RUN_TEST(test_probe_enable_requires_exact_signature);
  RUN_TEST(test_probe_region_extent);
  RUN_TEST(test_probe_is_single_register);
  RUN_TEST(test_write_sram_ignored);

  // Motor control
  RUN_TEST(test_motor_requires_signature_enabled);
  RUN_TEST(test_motor_start_fires_callback);
  RUN_TEST(test_motor_stop_fires_callback);
  RUN_TEST(test_motor_follows_low_bit_of_last_byte);
  RUN_TEST(test_motor_uses_last_byte_not_first);
  RUN_TEST(test_motor_callback_edge_triggered);
  RUN_TEST(test_motor_stop_when_already_off);
  RUN_TEST(test_motor_region_extent);
  RUN_TEST(test_motor_write_without_callback);

  // Wire-level sequences
  RUN_TEST(test_wire_probe_read_before_enable_returns_zeros);
  RUN_TEST(test_wire_probe_enable_then_read_returns_signature);
  RUN_TEST(test_wire_motor_start_stop);

  return UNITY_END();
}
