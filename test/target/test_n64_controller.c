#include <string.h>

#include <joybus/bus.h>
#include <joybus/checksum.h>
#include <joybus/commands.h>
#include <joybus/errors.h>
#include <joybus/identify.h>
#include <joybus/target.h>
#include <joybus/common/n64_controller.h>
#include <joybus/target/n64_controller.h>
#include <joybus/target/n64_pak.h>

#include "unity.h"

#include "harness.h"

// The controller target under test
static struct joybus_target_n64_controller controller;

// Spy for the reset callback
static int reset_count;
static void on_reset(struct joybus_target_n64_controller *c)
{
  reset_count++;
}

// Fake pak that records reads and writes
static int read_block_count;
static uint16_t read_block_addr;
static int write_block_count;
static uint16_t write_block_addr;
static int write_block_seq;
static uint8_t write_block_data[JOYBUS_PAK_BLOCK_SIZE];

static void fake_read_block(struct joybus_target_n64_pak *pak, uint16_t addr, uint8_t buf[JOYBUS_PAK_BLOCK_SIZE])
{
  read_block_count++;
  read_block_addr = addr;

  // Fill the block with a recognizable pattern
  for (uint8_t i = 0; i < JOYBUS_PAK_BLOCK_SIZE; i++) {
    buf[i] = i;
  }
}

static void fake_write_block(struct joybus_target_n64_pak *pak, uint16_t addr, const uint8_t buf[JOYBUS_PAK_BLOCK_SIZE])
{
  write_block_count++;
  write_block_addr = addr;
  write_block_seq  = ++event_seq;
  memcpy(write_block_data, buf, JOYBUS_PAK_BLOCK_SIZE);
}

static const struct joybus_target_n64_pak_api fake_pak_api = {
  .read_block  = fake_read_block,
  .write_block = fake_write_block,
};

static struct joybus_target_n64_pak pak;

// Build an aligned block address with its checksum in the low 5 bits
static uint16_t valid_pak_addr(uint16_t block_addr)
{
  return block_addr | joybus_address_checksum(block_addr >> 5);
}

// Build a full pak write command for the given wire address and payload
static void build_pak_write(uint8_t command[JOYBUS_CMD_N64_PAK_WRITE_TX], uint16_t addr,
                            const uint8_t payload[JOYBUS_PAK_BLOCK_SIZE])
{
  command[0] = JOYBUS_CMD_N64_PAK_WRITE;
  command[1] = addr >> 8;
  command[2] = addr & 0xFF;
  memcpy(&command[3], payload, JOYBUS_PAK_BLOCK_SIZE);
}

// A payload of distinct bytes for write tests
static void fill_payload(uint8_t payload[JOYBUS_PAK_BLOCK_SIZE])
{
  for (uint8_t i = 0; i < JOYBUS_PAK_BLOCK_SIZE; i++) {
    payload[i] = 0x10 + i;
  }
}

void setUp(void)
{
  // init zeroes the whole struct itself, including the registered flag
  joybus_target_n64_controller_init(&controller);

  // Set up the fake pak, detached until a test attaches it
  pak.api = &fake_pak_api;

  // Point the harness at the controller and clear recorded responses
  harness_reset(JOYBUS_TARGET(&controller));

  // Reset the callback and pak spies
  reset_count       = 0;
  read_block_count  = 0;
  read_block_addr   = 0;
  write_block_count = 0;
  write_block_addr  = 0;
  write_block_seq   = 0;
  memset(write_block_data, 0, sizeof(write_block_data));
}

void tearDown(void)
{
}

// ---------------------------------------------------------------------------
// State API (no command bytes involved)
// ---------------------------------------------------------------------------

// Test the initial state set up by init
static void test_init_defaults(void)
{
  uint8_t expected_id[] = {0x05, 0x00, 0x02};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_id, &controller.id, sizeof(expected_id));

  TEST_ASSERT_EQUAL_HEX16(0x0000, controller.input.buttons);
  TEST_ASSERT_EQUAL_HEX8(0x00, controller.input.stick_x);
  TEST_ASSERT_EQUAL_HEX8(0x00, controller.input.stick_y);
  TEST_ASSERT_NULL(controller.pak);
}

// Test that attaching a pak before registration reports it as present, with no change flag
static void test_attach_pak_before_registration(void)
{
  joybus_target_n64_controller_attach_pak(&controller, &pak);

  TEST_ASSERT_EQUAL_HEX8(JOYBUS_STATUS_N64_PAK_PRESENT, joybus_id_get_status(&controller.id));
}

// Test that attaching a pak while registered reports it as changed (present and pulled together)
static void test_attach_pak_while_registered(void)
{
  // Simulate "powered on" without involving a bus
  controller.base.registered = true;

  joybus_target_n64_controller_attach_pak(&controller, &pak);

  TEST_ASSERT_EQUAL_HEX8(JOYBUS_STATUS_N64_PAK_PRESENT | JOYBUS_STATUS_N64_PAK_PULLED,
                         joybus_id_get_status(&controller.id));
}

// Test that detaching a pak reports it as pulled
static void test_detach_pak(void)
{
  joybus_target_n64_controller_attach_pak(&controller, &pak);
  joybus_target_n64_controller_detach_pak(&controller);

  TEST_ASSERT_NULL(controller.pak);
  TEST_ASSERT_EQUAL_HEX8(JOYBUS_STATUS_N64_PAK_PULLED, joybus_id_get_status(&controller.id));
}

// ---------------------------------------------------------------------------
// Identify (0x00)
// ---------------------------------------------------------------------------

// Test that identify responds at the first byte with the controller ID
static void test_identify(void)
{
  uint8_t command[] = {JOYBUS_CMD_IDENTIFY};
  send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL(1, response.count);
  TEST_ASSERT_EQUAL(1, response.at_byte);
  TEST_ASSERT_EQUAL(JOYBUS_CMD_IDENTIFY_RX, response.len);

  uint8_t expected[] = {0x05, 0x00, 0x02};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));
}

// Test that identify reports "pak changed" exactly once, then "pak present"
static void test_identify_acks_pak_changed(void)
{
  controller.base.registered = true;
  joybus_target_n64_controller_attach_pak(&controller, &pak);

  uint8_t command[] = {JOYBUS_CMD_IDENTIFY};

  // The first identify reports the change
  send_command(command, sizeof(command));
  TEST_ASSERT_EQUAL_HEX8(JOYBUS_STATUS_N64_PAK_PRESENT | JOYBUS_STATUS_N64_PAK_PULLED, response.data[2]);

  // Subsequent identifies report the pak as present
  send_command(command, sizeof(command));
  TEST_ASSERT_EQUAL_HEX8(JOYBUS_STATUS_N64_PAK_PRESENT, response.data[2]);
}

// Test that a latched checksum error is reported by one identify and cleared by the next
static void test_identify_clears_checksum_error(void)
{
  joybus_target_n64_controller_attach_pak(&controller, &pak);

  // A pak read with a bad address checksum latches the error flag
  uint8_t bad_read[] = {JOYBUS_CMD_N64_PAK_READ, 0x80, 0x00};
  send_command(bad_read, sizeof(bad_read));

  uint8_t command[] = {JOYBUS_CMD_IDENTIFY};
  send_command(command, sizeof(command));
  TEST_ASSERT_EQUAL_HEX8(JOYBUS_STATUS_N64_PAK_PRESENT | JOYBUS_STATUS_N64_ADDR_CHECKSUM_ERROR, response.data[2]);

  send_command(command, sizeof(command));
  TEST_ASSERT_EQUAL_HEX8(JOYBUS_STATUS_N64_PAK_PRESENT, response.data[2]);
}

// ---------------------------------------------------------------------------
// Reset (0xFF)
// ---------------------------------------------------------------------------

// Test that reset responds with the ID and fires the reset callback once
static void test_reset(void)
{
  joybus_target_n64_controller_set_reset_cb(&controller, on_reset);

  uint8_t command[] = {JOYBUS_CMD_RESET};
  send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL(1, response.count);
  TEST_ASSERT_EQUAL(1, response.at_byte);
  TEST_ASSERT_EQUAL(JOYBUS_CMD_RESET_RX, response.len);

  uint8_t expected[] = {0x05, 0x00, 0x02};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));
  TEST_ASSERT_EQUAL(1, reset_count);
}

// Test that reset is safe when no callback is registered
static void test_reset_without_callback(void)
{
  uint8_t command[] = {JOYBUS_CMD_RESET};
  send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL(1, response.count);
  TEST_ASSERT_EQUAL(JOYBUS_CMD_RESET_RX, response.len);
}

// ---------------------------------------------------------------------------
// Read (0x01)
// ---------------------------------------------------------------------------

// Test that read responds at the first byte with the current input state
static void test_read_returns_input_state(void)
{
  controller.input.buttons = JOYBUS_N64_BUTTON_A | JOYBUS_N64_BUTTON_C_UP;
  controller.input.stick_x = 0x12;
  controller.input.stick_y = -18; // 0xEE on the wire

  uint8_t command[] = {JOYBUS_CMD_N64_READ};
  send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL(1, response.count);
  TEST_ASSERT_EQUAL(1, response.at_byte);
  TEST_ASSERT_EQUAL(JOYBUS_CMD_N64_READ_RX, response.len);

  uint8_t expected[] = {0x80, 0x08, 0x12, 0xEE};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));
}

// ---------------------------------------------------------------------------
// Stick origin and recalibration
// ---------------------------------------------------------------------------

// Test that reset re-samples the stick origin to the current physical position
static void test_reset_resamples_stick_origin(void)
{
  uint8_t read[]  = {JOYBUS_CMD_N64_READ};
  uint8_t reset[] = {JOYBUS_CMD_RESET};

  // Stick deflected right; with the default centered origin it reads through
  controller.input.stick_x = 50;
  send_command(read, sizeof(read));
  TEST_ASSERT_EQUAL_HEX8(50, response.data[2]);

  // Reset re-samples the neutral to wherever the stick currently is
  send_command(reset, sizeof(reset));

  // The same physical position now reads as centered
  send_command(read, sizeof(read));
  TEST_ASSERT_EQUAL_HEX8(0, response.data[2]);
}

// Test that calibrate snapshots the current stick position as the origin, so the app can choose when the power-on
// neutral is sampled
static void test_calibrate_snapshots_origin(void)
{
  uint8_t read[] = {JOYBUS_CMD_N64_READ};

  // Stick deflected; calibrate adopts it as the neutral
  controller.input.stick_x = 50;
  joybus_target_n64_controller_calibrate(&controller);

  // The same physical position now reads as centered
  send_command(read, sizeof(read));
  TEST_ASSERT_EQUAL_HEX8(0, response.data[2]);

  // Moving past the new origin reads as a positive delta
  controller.input.stick_x = 80;
  send_command(read, sizeof(read));
  TEST_ASSERT_EQUAL_HEX8(30, response.data[2]);
}

// Test that, after a reset moves the origin, reads report the signed delta from it
static void test_read_reports_delta_from_resampled_origin(void)
{
  uint8_t read[]  = {JOYBUS_CMD_N64_READ};
  uint8_t reset[] = {JOYBUS_CMD_RESET};

  // Re-sample the origin with the stick held at +50
  controller.input.stick_x = 50;
  send_command(reset, sizeof(reset));

  // Past the new origin reads as a positive delta
  controller.input.stick_x = 80;
  send_command(read, sizeof(read));
  TEST_ASSERT_EQUAL_HEX8(30, response.data[2]);

  // Back at the old center reads as a negative delta (mirror of the offset)
  controller.input.stick_x = 0;
  send_command(read, sizeof(read));
  TEST_ASSERT_EQUAL_HEX8(0xCE, response.data[2]); // 0 - 50 = -50
}

// Test that holding L+R+Start raises the RST flag and suppresses the Start button
static void test_combo_raises_rst_and_suppresses_start(void)
{
  controller.input.buttons = JOYBUS_N64_BUTTON_L | JOYBUS_N64_BUTTON_R | JOYBUS_N64_BUTTON_START;

  uint8_t read[] = {JOYBUS_CMD_N64_READ};
  send_command(read, sizeof(read));

  // RST (byte 1 bit 7) is raised, Start (byte 0 bit 4) is suppressed, L+R pass through
  uint8_t expected[] = {0x00, 0xB0, 0x00, 0x00};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));
}

// Test that the L+R+Start combo re-samples the origin, like a reset
static void test_combo_resamples_origin(void)
{
  uint8_t read[] = {JOYBUS_CMD_N64_READ};

  // Hold the combo with the stick deflected: it re-zeros to the held position
  controller.input.stick_x = 50;
  controller.input.buttons = JOYBUS_N64_BUTTON_L | JOYBUS_N64_BUTTON_R | JOYBUS_N64_BUTTON_START;
  send_command(read, sizeof(read));
  TEST_ASSERT_EQUAL_HEX8(0, response.data[2]);

  // Release the combo; the moved origin persists, so center reads negative
  controller.input.buttons = 0;
  controller.input.stick_x = 0;
  send_command(read, sizeof(read));
  TEST_ASSERT_EQUAL_HEX8(0xCE, response.data[2]); // 0 - 50 = -50
}

// ---------------------------------------------------------------------------
// Pak read (0x02)
// ---------------------------------------------------------------------------

// Test that a valid pak read responds at the third byte with the pak's data
// and its CRC, passing the block-aligned address to the pak
static void test_pak_read_returns_pak_data(void)
{
  joybus_target_n64_controller_attach_pak(&controller, &pak);

  uint16_t addr     = valid_pak_addr(0x8000);
  uint8_t command[] = {JOYBUS_CMD_N64_PAK_READ, addr >> 8, addr & 0xFF};
  send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL(1, response.count);
  TEST_ASSERT_EQUAL(3, response.at_byte);
  TEST_ASSERT_EQUAL(JOYBUS_CMD_N64_PAK_READ_RX, response.len);

  // The fake pak was asked for one block at the aligned address
  TEST_ASSERT_EQUAL(1, read_block_count);
  TEST_ASSERT_EQUAL_HEX16(0x8000, read_block_addr);

  // The response is the pak's pattern followed by its data checksum
  uint8_t expected[JOYBUS_CMD_N64_PAK_READ_RX];
  for (uint8_t i = 0; i < JOYBUS_PAK_BLOCK_SIZE; i++) {
    expected[i] = i;
  }
  expected[JOYBUS_PAK_BLOCK_SIZE] = joybus_data_checksum(expected, JOYBUS_PAK_BLOCK_SIZE);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));
}

// Test that a pak read with no pak attached returns zeroes with the "no pak" CRC
static void test_pak_read_no_pak(void)
{
  uint16_t addr     = valid_pak_addr(0x8000);
  uint8_t command[] = {JOYBUS_CMD_N64_PAK_READ, addr >> 8, addr & 0xFF};
  send_command(command, sizeof(command));

  uint8_t expected[JOYBUS_CMD_N64_PAK_READ_RX] = {0};
  expected[JOYBUS_PAK_BLOCK_SIZE]              = 0xFF;
  TEST_ASSERT_EQUAL(JOYBUS_CMD_N64_PAK_READ_RX, response.len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));
}

// Test that a pak read is refused while the pak change has not been acknowledged by an identify
static void test_pak_read_refused_while_pak_changed(void)
{
  controller.base.registered = true;
  joybus_target_n64_controller_attach_pak(&controller, &pak);

  uint16_t addr     = valid_pak_addr(0x8000);
  uint8_t command[] = {JOYBUS_CMD_N64_PAK_READ, addr >> 8, addr & 0xFF};
  send_command(command, sizeof(command));

  // The pak is never consulted; the "no pak" response is returned
  TEST_ASSERT_EQUAL(0, read_block_count);
  uint8_t expected[JOYBUS_CMD_N64_PAK_READ_RX] = {0};
  expected[JOYBUS_PAK_BLOCK_SIZE]              = 0xFF;
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));
}

// Test that a pak read with a bad address checksum is refused and latches the checksum error flag
static void test_pak_read_bad_checksum(void)
{
  joybus_target_n64_controller_attach_pak(&controller, &pak);

  // 0x8000 has its checksum bits zeroed, which is not the checksum of 0x400
  uint8_t bad_read[] = {JOYBUS_CMD_N64_PAK_READ, 0x80, 0x00};
  send_command(bad_read, sizeof(bad_read));

  TEST_ASSERT_EQUAL(0, read_block_count);
  TEST_ASSERT_TRUE(joybus_id_get_status(&controller.id) & JOYBUS_STATUS_N64_ADDR_CHECKSUM_ERROR);

  uint8_t expected[JOYBUS_CMD_N64_PAK_READ_RX] = {0};
  expected[JOYBUS_PAK_BLOCK_SIZE]              = 0xFF;
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response.data, sizeof(expected));
}

// Test that a subsequent valid pak read clears the checksum error flag
static void test_pak_read_valid_clears_checksum_error(void)
{
  joybus_target_n64_controller_attach_pak(&controller, &pak);

  uint8_t bad_read[] = {JOYBUS_CMD_N64_PAK_READ, 0x80, 0x00};
  send_command(bad_read, sizeof(bad_read));
  TEST_ASSERT_TRUE(joybus_id_get_status(&controller.id) & JOYBUS_STATUS_N64_ADDR_CHECKSUM_ERROR);

  uint16_t addr       = valid_pak_addr(0x8000);
  uint8_t good_read[] = {JOYBUS_CMD_N64_PAK_READ, addr >> 8, addr & 0xFF};
  send_command(good_read, sizeof(good_read));
  TEST_ASSERT_FALSE(joybus_id_get_status(&controller.id) & JOYBUS_STATUS_N64_ADDR_CHECKSUM_ERROR);
}

// ---------------------------------------------------------------------------
// Pak write (0x03)
// ---------------------------------------------------------------------------

// Test that a valid pak write responds at the final byte with the payload CRC, and hands the payload to the pak only
// after the response is sent
static void test_pak_write_commits_to_pak(void)
{
  joybus_target_n64_controller_attach_pak(&controller, &pak);

  uint8_t payload[JOYBUS_PAK_BLOCK_SIZE];
  fill_payload(payload);

  uint8_t command[JOYBUS_CMD_N64_PAK_WRITE_TX];
  build_pak_write(command, valid_pak_addr(0x8000), payload);
  send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL(1, response.count);
  TEST_ASSERT_EQUAL(JOYBUS_CMD_N64_PAK_WRITE_TX, response.at_byte);
  TEST_ASSERT_EQUAL(JOYBUS_CMD_N64_PAK_WRITE_RX, response.len);
  TEST_ASSERT_EQUAL_HEX8(joybus_data_checksum(payload, sizeof(payload)), response.data[0]);

  // The payload reached the pak at the aligned address, after the response
  TEST_ASSERT_EQUAL(1, write_block_count);
  TEST_ASSERT_EQUAL_HEX16(0x8000, write_block_addr);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(payload, write_block_data, sizeof(payload));
  TEST_ASSERT_TRUE(response.seq < write_block_seq);
}

// Test that a pak write with no pak attached responds with the inverted "no pak" CRC and never reaches a pak
static void test_pak_write_no_pak(void)
{
  uint8_t payload[JOYBUS_PAK_BLOCK_SIZE];
  fill_payload(payload);

  uint8_t command[JOYBUS_CMD_N64_PAK_WRITE_TX];
  build_pak_write(command, valid_pak_addr(0x8000), payload);
  send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL(JOYBUS_CMD_N64_PAK_WRITE_RX, response.len);
  TEST_ASSERT_EQUAL_HEX8(joybus_data_checksum(payload, sizeof(payload)) ^ 0xFF, response.data[0]);
  TEST_ASSERT_EQUAL(0, write_block_count);
}

// Test that a pak write is refused while the pak change has not been acknowledged by an identify
static void test_pak_write_refused_while_pak_changed(void)
{
  controller.base.registered = true;
  joybus_target_n64_controller_attach_pak(&controller, &pak);

  uint8_t payload[JOYBUS_PAK_BLOCK_SIZE];
  fill_payload(payload);

  uint8_t command[JOYBUS_CMD_N64_PAK_WRITE_TX];
  build_pak_write(command, valid_pak_addr(0x8000), payload);
  send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL_HEX8(joybus_data_checksum(payload, sizeof(payload)) ^ 0xFF, response.data[0]);
  TEST_ASSERT_EQUAL(0, write_block_count);
}

// Test that a pak write with a bad address checksum is refused and latches the checksum error flag
static void test_pak_write_bad_checksum(void)
{
  joybus_target_n64_controller_attach_pak(&controller, &pak);

  uint8_t payload[JOYBUS_PAK_BLOCK_SIZE];
  fill_payload(payload);

  // 0x8000 has its checksum bits zeroed, which is not the checksum of 0x400
  uint8_t command[JOYBUS_CMD_N64_PAK_WRITE_TX];
  build_pak_write(command, 0x8000, payload);
  send_command(command, sizeof(command));

  TEST_ASSERT_EQUAL_HEX8(joybus_data_checksum(payload, sizeof(payload)) ^ 0xFF, response.data[0]);
  TEST_ASSERT_EQUAL(0, write_block_count);
  TEST_ASSERT_TRUE(joybus_id_get_status(&controller.id) & JOYBUS_STATUS_N64_ADDR_CHECKSUM_ERROR);
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

// Test the full pak lifecycle as the host sees it (absent -> changed -> present -> pulled)
static void test_pak_lifecycle(void)
{
  controller.base.registered = true;
  uint8_t identify[]         = {JOYBUS_CMD_IDENTIFY};

  // No pak after power-on
  send_command(identify, sizeof(identify));
  TEST_ASSERT_EQUAL_HEX8(JOYBUS_STATUS_N64_PAK_PULLED, response.data[2]);

  // Hotplug: changed once, then present
  joybus_target_n64_controller_attach_pak(&controller, &pak);
  send_command(identify, sizeof(identify));
  TEST_ASSERT_EQUAL_HEX8(JOYBUS_STATUS_N64_PAK_PRESENT | JOYBUS_STATUS_N64_PAK_PULLED, response.data[2]);
  send_command(identify, sizeof(identify));
  TEST_ASSERT_EQUAL_HEX8(JOYBUS_STATUS_N64_PAK_PRESENT, response.data[2]);

  // Unplug: pulled, and it stays that way
  joybus_target_n64_controller_detach_pak(&controller);
  send_command(identify, sizeof(identify));
  TEST_ASSERT_EQUAL_HEX8(JOYBUS_STATUS_N64_PAK_PULLED, response.data[2]);
  send_command(identify, sizeof(identify));
  TEST_ASSERT_EQUAL_HEX8(JOYBUS_STATUS_N64_PAK_PULLED, response.data[2]);
}

// Test that pak reads start working after an identify acknowledges the hotplugged pak
static void test_pak_read_works_after_changed_acked(void)
{
  controller.base.registered = true;
  joybus_target_n64_controller_attach_pak(&controller, &pak);

  uint16_t addr      = valid_pak_addr(0x8000);
  uint8_t pak_read[] = {JOYBUS_CMD_N64_PAK_READ, addr >> 8, addr & 0xFF};
  uint8_t identify[] = {JOYBUS_CMD_IDENTIFY};

  // Refused before the change is acknowledged
  send_command(pak_read, sizeof(pak_read));
  TEST_ASSERT_EQUAL(0, read_block_count);

  // Acknowledge via identify, then the read is honored
  send_command(identify, sizeof(identify));
  send_command(pak_read, sizeof(pak_read));
  TEST_ASSERT_EQUAL(1, read_block_count);
  TEST_ASSERT_EQUAL_HEX16(0x8000, read_block_addr);
}

int main(void)
{
  UNITY_BEGIN();

  // State API
  RUN_TEST(test_init_defaults);
  RUN_TEST(test_attach_pak_before_registration);
  RUN_TEST(test_attach_pak_while_registered);
  RUN_TEST(test_detach_pak);

  // Identify
  RUN_TEST(test_identify);
  RUN_TEST(test_identify_acks_pak_changed);
  RUN_TEST(test_identify_clears_checksum_error);

  // Reset
  RUN_TEST(test_reset);
  RUN_TEST(test_reset_without_callback);

  // Read
  RUN_TEST(test_read_returns_input_state);

  // Stick origin and recalibration
  RUN_TEST(test_calibrate_snapshots_origin);
  RUN_TEST(test_reset_resamples_stick_origin);
  RUN_TEST(test_read_reports_delta_from_resampled_origin);
  RUN_TEST(test_combo_raises_rst_and_suppresses_start);
  RUN_TEST(test_combo_resamples_origin);

  // Pak read
  RUN_TEST(test_pak_read_returns_pak_data);
  RUN_TEST(test_pak_read_no_pak);
  RUN_TEST(test_pak_read_refused_while_pak_changed);
  RUN_TEST(test_pak_read_bad_checksum);
  RUN_TEST(test_pak_read_valid_clears_checksum_error);

  // Pak write
  RUN_TEST(test_pak_write_commits_to_pak);
  RUN_TEST(test_pak_write_no_pak);
  RUN_TEST(test_pak_write_refused_while_pak_changed);
  RUN_TEST(test_pak_write_bad_checksum);

  // Unsupported commands
  RUN_TEST(test_unknown_command_not_supported);

  // Cross-command sequences
  RUN_TEST(test_pak_lifecycle);
  RUN_TEST(test_pak_read_works_after_changed_acked);

  return UNITY_END();
}
