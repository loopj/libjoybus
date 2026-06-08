#include <joybus/checksum.h>

#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

// Test joybus_data_checksum_update against various single-byte inputs
static void test_data_checksum_update_known_bytes()
{
  TEST_ASSERT_EQUAL_HEX8(0x00, joybus_data_checksum_update(0, 0x00));
  TEST_ASSERT_EQUAL_HEX8(0x85, joybus_data_checksum_update(0, 0x01));
  TEST_ASSERT_EQUAL_HEX8(0x89, joybus_data_checksum_update(0, 0x80));
}

// Test joybus_data_checksum over an empty buffer
static void test_data_checksum_empty_buffer()
{
  TEST_ASSERT_EQUAL_HEX8(0x00, joybus_data_checksum(NULL, 0));
}

// Test joybus_data_checksum over a single-byte buffer
static void test_data_checksum_single_byte()
{
  uint8_t buf = 0x80;
  TEST_ASSERT_EQUAL_HEX8(0x89, joybus_data_checksum(&buf, 1));
}

// Test joybus_data_checksum over a multi-byte buffer
static void test_data_checksum_multi_byte()
{
  uint8_t buf[] = {0x80, 0x80};
  TEST_ASSERT_EQUAL_HEX8(0x36, joybus_data_checksum(buf, sizeof(buf)));
}

// Test address_checksum against various known values
static void test_address_checksum_known()
{
  TEST_ASSERT_EQUAL_HEX8(0x01, joybus_address_checksum(0x8000 >> 5));
  TEST_ASSERT_EQUAL_HEX8(0x00, joybus_address_checksum(0x0000 >> 5));
  TEST_ASSERT_EQUAL_HEX8(0x14, joybus_address_checksum(0x8020 >> 5));
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();

  RUN_TEST(test_data_checksum_update_known_bytes);
  RUN_TEST(test_data_checksum_empty_buffer);
  RUN_TEST(test_data_checksum_single_byte);
  RUN_TEST(test_data_checksum_multi_byte);

  RUN_TEST(test_address_checksum_known);

  return UNITY_END();
}
