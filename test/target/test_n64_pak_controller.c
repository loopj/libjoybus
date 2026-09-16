#include <string.h>

#include <joybus/bus.h>
#include <joybus/checksum.h>
#include <joybus/commands.h>
#include <joybus/errors.h>
#include <joybus/target.h>
#include <joybus/common/n64_pak_fs.h>
#include <joybus/target/n64_controller.h>
#include <joybus/target/n64_pak.h>
#include <joybus/target/n64_pak_controller.h>

#include "unity.h"

#include "harness.h"

// The controller pak under test, and a controller to host it for the wire tests
static struct joybus_target_n64_pak_controller pak_controller;
static struct joybus_target_n64_controller controller;

// Spy storage: records the last access and answers with a fill byte
static struct {
  int reads;
  int writes;
  uint8_t bank;
  uint16_t addr;
  uint8_t data[JOYBUS_N64_PAK_BLOCK_SIZE];
  uint8_t fill;
  int result;
} storage;

// Spies for the select and written callbacks
static struct {
  int selects;
  uint8_t bank;
} selected;

static struct {
  int count;
  uint8_t bank;
  uint16_t addr;
} written;

static int spy_read_block(struct joybus_target_n64_pak_controller *pak, uint8_t bank, uint16_t addr,
                          uint8_t buf[JOYBUS_N64_PAK_BLOCK_SIZE])
{
  TEST_ASSERT_EQUAL_PTR(&storage, pak->user_data);
  storage.reads++;
  storage.bank = bank;
  storage.addr = addr;
  memset(buf, storage.fill, JOYBUS_N64_PAK_BLOCK_SIZE);
  return storage.result;
}

static int spy_write_block(struct joybus_target_n64_pak_controller *pak, uint8_t bank, uint16_t addr,
                           const uint8_t buf[JOYBUS_N64_PAK_BLOCK_SIZE])
{
  TEST_ASSERT_EQUAL_PTR(&storage, pak->user_data);
  storage.writes++;
  storage.bank = bank;
  storage.addr = addr;
  memcpy(storage.data, buf, JOYBUS_N64_PAK_BLOCK_SIZE);
  return storage.result;
}

static void on_select(struct joybus_target_n64_pak_controller *pak, uint8_t bank)
{
  selected.selects++;
  selected.bank = bank;
}

static void on_written(struct joybus_target_n64_pak_controller *pak, uint8_t bank, uint16_t addr)
{
  written.count++;
  written.bank = bank;
  written.addr = addr;
}

// ---------------------------------------------------------------------------
// Direct pak-API helpers
// ---------------------------------------------------------------------------

// Read a block directly through the pak API
static int pak_read(uint16_t addr, uint8_t buf[JOYBUS_N64_PAK_BLOCK_SIZE])
{
  return pak_controller.base.api->read_block(&pak_controller.base, addr, buf);
}

// Write a block of 32 x `fill` directly through the pak API
static int pak_write_fill(uint16_t addr, uint8_t fill)
{
  uint8_t buf[JOYBUS_N64_PAK_BLOCK_SIZE];
  memset(buf, fill, sizeof(buf));
  return pak_controller.base.api->write_block(&pak_controller.base, addr, buf);
}

// Write a block whose ID bank count byte is `banks` directly through the pak API
static int pak_write_id(uint16_t addr, uint8_t banks)
{
  uint8_t buf[JOYBUS_N64_PAK_BLOCK_SIZE] = {0};
  buf[0x1A]                              = banks;
  return pak_controller.base.api->write_block(&pak_controller.base, addr, buf);
}

// Set the pak up with the given bank count over the spy storage, hosted in a fresh controller
static void make_pak(uint8_t banks)
{
  joybus_target_n64_pak_controller_init(&pak_controller, banks);
  joybus_target_n64_pak_controller_set_storage(&pak_controller, spy_read_block, spy_write_block, &storage);
  joybus_target_n64_pak_controller_set_select_cb(&pak_controller, on_select);
  joybus_target_n64_pak_controller_set_written_cb(&pak_controller, on_written);

  joybus_target_n64_controller_init(&controller);
  joybus_target_n64_controller_attach_pak(&controller, JOYBUS_TARGET_N64_PAK(&pak_controller));
  harness_reset(JOYBUS_TARGET(&controller));
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

void setUp(void)
{
  memset(&storage, 0, sizeof(storage));
  memset(&selected, 0, sizeof(selected));
  memset(&written, 0, sizeof(written));
  make_pak(16);
}

void tearDown(void)
{
}

// ---------------------------------------------------------------------------
// One bank pak
// ---------------------------------------------------------------------------

// Test that a one bank pak aliases the probe area onto the bottom of the bank
static void test_one_bank_aliases_probe_area()
{
  make_pak(1);
  uint8_t buf[JOYBUS_N64_PAK_BLOCK_SIZE];

  TEST_ASSERT_EQUAL(0, pak_read(0x8000, buf));
  TEST_ASSERT_EQUAL(1, storage.reads);
  TEST_ASSERT_EQUAL_HEX16(0x0000, storage.addr);

  TEST_ASSERT_EQUAL(0, pak_read(0xC020, buf));
  TEST_ASSERT_EQUAL_HEX16(0x4020, storage.addr);
}

// Test that a one bank pak never selects, a probe write lands in storage
static void test_one_bank_probe_write_is_storage()
{
  make_pak(1);

  TEST_ASSERT_EQUAL(0, pak_write_fill(0x8000, 0xFE));
  TEST_ASSERT_EQUAL(1, storage.writes);
  TEST_ASSERT_EQUAL(0, selected.selects);
  TEST_ASSERT_EQUAL_HEX16(0x0000, storage.addr);
  TEST_ASSERT_EQUAL(0, pak_controller.selected);
}

// ---------------------------------------------------------------------------
// Banked pak
// ---------------------------------------------------------------------------

// Test that a banked pak answers zeros in the probe area without touching storage
static void test_banked_probe_area_reads_zero()
{
  uint8_t buf[JOYBUS_N64_PAK_BLOCK_SIZE];
  memset(buf, 0xAA, sizeof(buf));
  storage.fill = 0x55;

  TEST_ASSERT_EQUAL(0, pak_read(0x8000, buf));
  TEST_ASSERT_EACH_EQUAL_HEX8(0x00, buf, sizeof(buf));
  TEST_ASSERT_EQUAL(0, storage.reads);

  TEST_ASSERT_EQUAL(0, pak_read(0xC000, buf));
  TEST_ASSERT_EQUAL(0, storage.reads);
}

// Test that a bank select in range switches the bank reads and writes go to
static void test_banked_select_in_range()
{
  uint8_t buf[JOYBUS_N64_PAK_BLOCK_SIZE];

  TEST_ASSERT_EQUAL(0, pak_write_fill(0x8000, 5));
  TEST_ASSERT_EQUAL(5, pak_controller.selected);
  TEST_ASSERT_EQUAL(1, selected.selects);
  TEST_ASSERT_EQUAL(5, selected.bank);
  TEST_ASSERT_EQUAL(0, storage.writes);

  TEST_ASSERT_EQUAL(0, pak_read(0x0100, buf));
  TEST_ASSERT_EQUAL(5, storage.bank);
  TEST_ASSERT_EQUAL_HEX16(0x0100, storage.addr);

  TEST_ASSERT_EQUAL(0, pak_write_fill(0x0120, 0x11));
  TEST_ASSERT_EQUAL(5, storage.bank);
  TEST_ASSERT_EQUAL_HEX16(0x0120, storage.addr);
}

// Test that a select past the last bank is ignored, as an accessory probe relies on
static void test_banked_select_out_of_range_ignored()
{
  TEST_ASSERT_EQUAL(0, pak_write_fill(0x8000, 3));
  TEST_ASSERT_EQUAL(3, pak_controller.selected);

  // The values accessory type detection writes here
  TEST_ASSERT_EQUAL(0, pak_write_fill(0x8000, 0xFE));
  TEST_ASSERT_EQUAL(0, pak_write_fill(0x8000, 0x80));
  TEST_ASSERT_EQUAL(0, pak_write_fill(0x8000, 0x84));

  // And the first number past the edge
  TEST_ASSERT_EQUAL(0, pak_write_fill(0x8000, 16));

  TEST_ASSERT_EQUAL(3, pak_controller.selected);
  TEST_ASSERT_EQUAL(1, selected.selects);
  TEST_ASSERT_EQUAL(0, storage.writes);
}

// Test that a write to the motor region of a banked pak is swallowed
static void test_banked_motor_region_write_ignored()
{
  TEST_ASSERT_EQUAL(0, pak_write_fill(0xC000, 1));
  TEST_ASSERT_EQUAL(0, pak_controller.selected);
  TEST_ASSERT_EQUAL(0, selected.selects);
  TEST_ASSERT_EQUAL(0, storage.writes);
}

// Test that the select callback is optional
static void test_banked_select_without_callback()
{
  joybus_target_n64_pak_controller_set_select_cb(&pak_controller, NULL);

  TEST_ASSERT_EQUAL(0, pak_write_fill(0x8000, 2));
  TEST_ASSERT_EQUAL(2, pak_controller.selected);
}

// ---------------------------------------------------------------------------
// ID guard
// ---------------------------------------------------------------------------

// Test that an ID write naming another bank count is refused
static void test_id_write_with_other_bank_count_dropped()
{
  static const uint16_t id_addrs[] = {0x20, 0x60, 0x80, 0xC0};

  for (int i = 0; i < 4; i++) {
    TEST_ASSERT_EQUAL(-JOYBUS_ERR_INVALID_ARG, pak_write_id(id_addrs[i], 1));
    TEST_ASSERT_EQUAL(0, storage.writes);
    TEST_ASSERT_EQUAL(0, written.count);
  }
}

// Test that an ID write naming the pak's own bank count goes through
static void test_id_write_with_own_bank_count_kept()
{
  TEST_ASSERT_EQUAL(0, pak_write_id(0x20, 16));
  TEST_ASSERT_EQUAL(1, storage.writes);
  TEST_ASSERT_EQUAL_HEX16(0x0020, storage.addr);
}

// Test that the guard only watches bank 0, the same addresses elsewhere are data
static void test_id_guard_only_in_bank_zero()
{
  TEST_ASSERT_EQUAL(0, pak_write_fill(0x8000, 1));
  TEST_ASSERT_EQUAL(0, pak_write_id(0x20, 1));
  TEST_ASSERT_EQUAL(1, storage.writes);
  TEST_ASSERT_EQUAL(1, storage.bank);
}

// Test that a one bank pak's guard covers the aliased ID addresses too
static void test_id_guard_on_one_bank_alias()
{
  make_pak(1);

  TEST_ASSERT_EQUAL(-JOYBUS_ERR_INVALID_ARG, pak_write_id(0x8020, 16));
  TEST_ASSERT_EQUAL(0, storage.writes);
}

// ---------------------------------------------------------------------------
// Storage
// ---------------------------------------------------------------------------

// Test that a pak with no storage set answers busy
static void test_no_storage_is_busy()
{
  uint8_t buf[JOYBUS_N64_PAK_BLOCK_SIZE];
  joybus_target_n64_pak_controller_init(&pak_controller, 16);

  TEST_ASSERT_EQUAL(-JOYBUS_ERR_BUSY, pak_read(0x0100, buf));
  TEST_ASSERT_EQUAL(-JOYBUS_ERR_BUSY, pak_write_fill(0x0100, 0));
}

// Test that a busy backend is reported as busy, with no written event
static void test_storage_busy_propagates()
{
  uint8_t buf[JOYBUS_N64_PAK_BLOCK_SIZE];
  storage.result = -JOYBUS_ERR_BUSY;

  TEST_ASSERT_EQUAL(-JOYBUS_ERR_BUSY, pak_read(0x0100, buf));
  TEST_ASSERT_EQUAL(-JOYBUS_ERR_BUSY, pak_write_fill(0x0100, 0));
  TEST_ASSERT_EQUAL(0, written.count);
}

// Test that the written callback fires once storage has taken a write
static void test_written_callback_after_store()
{
  TEST_ASSERT_EQUAL(0, pak_write_fill(0x8000, 2));
  TEST_ASSERT_EQUAL(0, pak_write_fill(0x0140, 0x22));

  TEST_ASSERT_EQUAL(1, written.count);
  TEST_ASSERT_EQUAL(2, written.bank);
  TEST_ASSERT_EQUAL_HEX16(0x0140, written.addr);
}

// Test that memory storage reads and writes the caller's banks in place
static void test_memory_storage()
{
  static uint8_t memory[2 * JOYBUS_N64_PAK_BANK_SIZE];
  uint8_t buf[JOYBUS_N64_PAK_BLOCK_SIZE];

  memset(memory, 0, sizeof(memory));
  memset(&memory[JOYBUS_N64_PAK_BANK_SIZE + 0x0100], 0x77, JOYBUS_N64_PAK_BLOCK_SIZE);

  joybus_target_n64_pak_controller_init(&pak_controller, 2);
  joybus_target_n64_pak_controller_set_memory(&pak_controller, memory);
  joybus_target_n64_pak_controller_set_written_cb(&pak_controller, on_written);

  // Bank 1 reads back what was put there
  TEST_ASSERT_EQUAL(0, pak_write_fill(0x8000, 1));
  TEST_ASSERT_EQUAL(0, pak_read(0x0100, buf));
  TEST_ASSERT_EACH_EQUAL_HEX8(0x77, buf, sizeof(buf));

  // A write to bank 0 lands in the buffer and is reported
  TEST_ASSERT_EQUAL(0, pak_write_fill(0x8000, 0));
  TEST_ASSERT_EQUAL(0, pak_write_fill(0x7FE0, 0x99));
  TEST_ASSERT_EACH_EQUAL_HEX8(0x99, &memory[0x7FE0], JOYBUS_N64_PAK_BLOCK_SIZE);
  TEST_ASSERT_EQUAL(1, written.count);
  TEST_ASSERT_EQUAL(0, written.bank);
  TEST_ASSERT_EQUAL_HEX16(0x7FE0, written.addr);
}

// ---------------------------------------------------------------------------
// Wire
// ---------------------------------------------------------------------------

// Test that a read reaches the console with the backend's data and a valid checksum
static void test_wire_read_carries_storage_data()
{
  storage.fill = 0x5A;

  wire_pak_read(0x0100);

  TEST_ASSERT_EQUAL(1, response.count);
  TEST_ASSERT_EQUAL(JOYBUS_CMD_N64_PAK_READ_RX, response.len);
  TEST_ASSERT_EACH_EQUAL_HEX8(0x5A, response.data, JOYBUS_N64_PAK_BLOCK_SIZE);
  TEST_ASSERT_EQUAL_HEX8(joybus_data_checksum(response.data, JOYBUS_N64_PAK_BLOCK_SIZE),
                         response.data[JOYBUS_N64_PAK_BLOCK_SIZE]);
}

// Test that a busy backend reaches the console as the no-pak checksum
static void test_wire_read_busy_inverts_checksum()
{
  storage.result = -JOYBUS_ERR_BUSY;

  wire_pak_read(0x0100);

  TEST_ASSERT_EQUAL(1, response.count);
  TEST_ASSERT_EACH_EQUAL_HEX8(0x00, response.data, JOYBUS_N64_PAK_BLOCK_SIZE);
  TEST_ASSERT_EQUAL_HEX8(0xFF, response.data[JOYBUS_N64_PAK_BLOCK_SIZE]);
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();

  RUN_TEST(test_one_bank_aliases_probe_area);
  RUN_TEST(test_one_bank_probe_write_is_storage);

  RUN_TEST(test_banked_probe_area_reads_zero);
  RUN_TEST(test_banked_select_in_range);
  RUN_TEST(test_banked_select_out_of_range_ignored);
  RUN_TEST(test_banked_motor_region_write_ignored);
  RUN_TEST(test_banked_select_without_callback);

  RUN_TEST(test_id_write_with_other_bank_count_dropped);
  RUN_TEST(test_id_write_with_own_bank_count_kept);
  RUN_TEST(test_id_guard_only_in_bank_zero);
  RUN_TEST(test_id_guard_on_one_bank_alias);

  RUN_TEST(test_no_storage_is_busy);
  RUN_TEST(test_storage_busy_propagates);
  RUN_TEST(test_written_callback_after_store);
  RUN_TEST(test_memory_storage);

  RUN_TEST(test_wire_read_carries_storage_data);
  RUN_TEST(test_wire_read_busy_inverts_checksum);

  return UNITY_END();
}
