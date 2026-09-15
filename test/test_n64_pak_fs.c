#include <string.h>

#include <joybus/errors.h>
#include <joybus/common/n64_pak_fs.h>

#include "unity.h"

// A bank to format, and a copy of an ID block a console formatted for sixteen
// banks, as read back from a real pak
static uint8_t bank0[JOYBUS_N64_PAK_BANK_SIZE];
static const uint8_t console_id[32] = {0x00, 0x27, 0x00, 0x00, 0x00, 0x03, 0x23, 0xc6, 0x04, 0x3c, 0x98,
                                       0x69, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x01, 0x10, 0x00, 0xd0, 0x96, 0x2f, 0x5c};

void setUp(void)
{
  memset(bank0, 0xFF, sizeof(bank0));
}

void tearDown(void)
{
}

// The ID checksums as the console computes them, independently of the library
static void console_id_checksums(const uint8_t *id, uint16_t *sum, uint16_t *inverted)
{
  *sum      = 0;
  *inverted = 0;
  for (int i = 0; i < 28; i += 2) {
    uint16_t half = (uint16_t)(id[i] << 8 | id[i + 1]);
    *sum += half;
    *inverted += (uint16_t)~half;
  }
}

// Test that erased flash is not a filesystem
static void test_valid_rejects_erased()
{
  TEST_ASSERT_FALSE(joybus_n64_pak_fs_valid(bank0, 1));
  TEST_ASSERT_FALSE(joybus_n64_pak_fs_valid(bank0, 16));
}

// Test that a console-written ID is accepted for its bank count only
static void test_valid_accepts_console_id()
{
  memcpy(&bank0[0x20], console_id, sizeof(console_id));

  TEST_ASSERT_TRUE(joybus_n64_pak_fs_valid(bank0, 16));
  TEST_ASSERT_FALSE(joybus_n64_pak_fs_valid(bank0, 1));
}

// Test that any one intact copy is enough
static void test_valid_accepts_any_copy()
{
  memcpy(&bank0[0xC0], console_id, sizeof(console_id));

  TEST_ASSERT_TRUE(joybus_n64_pak_fs_valid(bank0, 16));
}

// Test that a corrupt checksum is rejected
static void test_valid_rejects_bad_checksum()
{
  memcpy(&bank0[0x20], console_id, sizeof(console_id));
  bank0[0x20 + 0x1C] ^= 0x01;

  TEST_ASSERT_FALSE(joybus_n64_pak_fs_valid(bank0, 16));
}

// Test that a format covers the system area and nothing more
static void test_format_extent()
{
  TEST_ASSERT_EQUAL((3 + 2 * 16) * 256, joybus_n64_pak_fs_format(bank0, 16, 0));
  TEST_ASSERT_EQUAL_HEX8(0xFF, bank0[(3 + 2 * 16) * 256]);
  TEST_ASSERT_EQUAL_HEX8(0xFF, bank0[JOYBUS_N64_PAK_BANK_SIZE - 1]);
}

// Test that a bank count outside the supported range is rejected without writing
static void test_format_rejects_bad_bank_count()
{
  TEST_ASSERT_EQUAL(-JOYBUS_ERR_INVALID_ARG, joybus_n64_pak_fs_format(bank0, 0, 0));
  TEST_ASSERT_EQUAL(-JOYBUS_ERR_INVALID_ARG, joybus_n64_pak_fs_format(bank0, JOYBUS_N64_PAK_FS_MAX_BANKS + 1, 0));
  TEST_ASSERT_EQUAL_HEX8(0xFF, bank0[0]);
  TEST_ASSERT_FALSE(joybus_n64_pak_fs_valid(bank0, 0));
}

// Test that the largest bank count leaves one data page in the first bank
static void test_format_max_banks()
{
  TEST_ASSERT_EQUAL(127 * 256, joybus_n64_pak_fs_format(bank0, JOYBUS_N64_PAK_FS_MAX_BANKS, 0));
  TEST_ASSERT_EQUAL_HEX8(0xFF, bank0[127 * 256]);

  // Only page 127 is free, so the inode sum is a single free marker
  const uint8_t *table = &bank0[1 * 256];
  TEST_ASSERT_EQUAL_HEX8(0x03, table[1]);
  TEST_ASSERT_EQUAL_HEX16(0x0000, (uint16_t)(table[126 * 2] << 8 | table[126 * 2 + 1]));
  TEST_ASSERT_EQUAL_HEX16(0x0003, (uint16_t)(table[127 * 2] << 8 | table[127 * 2 + 1]));
  TEST_ASSERT_TRUE(joybus_n64_pak_fs_valid(bank0, JOYBUS_N64_PAK_FS_MAX_BANKS));
}

// Test that every ID copy is written and passes the console's checks
static void test_format_ids()
{
  joybus_n64_pak_fs_format(bank0, 16, 0x12345678);

  static const uint16_t offsets[] = {0x20, 0x60, 0x80, 0xC0};
  for (int i = 0; i < 4; i++) {
    const uint8_t *id = &bank0[offsets[i]];
    uint16_t sum, inverted;
    console_id_checksums(id, &sum, &inverted);

    TEST_ASSERT_EQUAL_HEX16(sum, (uint16_t)(id[0x1C] << 8 | id[0x1D]));
    TEST_ASSERT_EQUAL_HEX16(inverted, (uint16_t)(id[0x1E] << 8 | id[0x1F]));
    TEST_ASSERT_EACH_EQUAL_HEX8(0xFF, &id[0x00], 4);
    TEST_ASSERT_EQUAL_HEX32(0x12345678, (uint32_t)id[0x04] << 24 | id[0x05] << 16 | id[0x06] << 8 | id[0x07]);
    TEST_ASSERT_EACH_EQUAL_HEX8(0x00, &id[0x08], 16);
    TEST_ASSERT_EQUAL_HEX16(0x0001, (uint16_t)(id[0x18] << 8 | id[0x19]));
    TEST_ASSERT_EQUAL_HEX8(16, id[0x1A]);
    TEST_ASSERT_EQUAL_HEX8(0x00, id[0x1B]);
  }

  TEST_ASSERT_TRUE(joybus_n64_pak_fs_valid(bank0, 16));
}

// Test the inode tables of a sixteen bank pak, whose checksums a console
// mounting one has been seen to accept
static void test_format_inodes_sixteen_banks()
{
  joybus_n64_pak_fs_format(bank0, 16, 0);

  // Bank 0's data starts at page 35, and 93 free pages sum to 0x17
  const uint8_t *table = &bank0[1 * 256];
  TEST_ASSERT_EQUAL_HEX8(0x17, table[1]);
  TEST_ASSERT_EQUAL_HEX16(0x0000, (uint16_t)(table[34 * 2] << 8 | table[34 * 2 + 1]));
  TEST_ASSERT_EQUAL_HEX16(0x0003, (uint16_t)(table[35 * 2] << 8 | table[35 * 2 + 1]));
  TEST_ASSERT_EQUAL_HEX16(0x0003, (uint16_t)(table[127 * 2] << 8 | table[127 * 2 + 1]));

  // A later bank's data starts at page 1, and 127 free pages sum to 0x7D
  const uint8_t *later = &bank0[2 * 256];
  TEST_ASSERT_EQUAL_HEX8(0x7D, later[1]);
  TEST_ASSERT_EQUAL_HEX16(0x0003, (uint16_t)(later[1 * 2] << 8 | later[1 * 2 + 1]));

  // The mirrors follow the tables, and the note table after them is empty
  for (int bank = 0; bank < 16; bank++)
    TEST_ASSERT_EQUAL_MEMORY(&bank0[(1 + bank) * 256], &bank0[(17 + bank) * 256], 256);
  for (int i = 33 * 256; i < 35 * 256; i++)
    TEST_ASSERT_EQUAL_HEX8(0x00, bank0[i]);
}

// Test the inode table of a one bank pak, the original's layout
static void test_format_inodes_one_bank()
{
  TEST_ASSERT_EQUAL(5 * 256, joybus_n64_pak_fs_format(bank0, 1, 0));

  // Data starts at page 5, and 123 free pages sum to 0x71
  const uint8_t *table = &bank0[1 * 256];
  TEST_ASSERT_EQUAL_HEX8(0x71, table[1]);
  TEST_ASSERT_EQUAL_HEX16(0x0000, (uint16_t)(table[4 * 2] << 8 | table[4 * 2 + 1]));
  TEST_ASSERT_EQUAL_HEX16(0x0003, (uint16_t)(table[5 * 2] << 8 | table[5 * 2 + 1]));
  TEST_ASSERT_EQUAL_MEMORY(&bank0[1 * 256], &bank0[2 * 256], 256);
  TEST_ASSERT_TRUE(joybus_n64_pak_fs_valid(bank0, 1));
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();

  RUN_TEST(test_valid_rejects_erased);
  RUN_TEST(test_valid_accepts_console_id);
  RUN_TEST(test_valid_accepts_any_copy);
  RUN_TEST(test_valid_rejects_bad_checksum);

  RUN_TEST(test_format_extent);
  RUN_TEST(test_format_rejects_bad_bank_count);
  RUN_TEST(test_format_max_banks);
  RUN_TEST(test_format_ids);
  RUN_TEST(test_format_inodes_sixteen_banks);
  RUN_TEST(test_format_inodes_one_bank);

  return UNITY_END();
}
