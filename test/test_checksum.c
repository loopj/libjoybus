#include <joybus/checksum.h>

#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

// Test joybus_crc8_update against various single-byte inputs
static void test_crc8_update_known_bytes()
{
  TEST_ASSERT_EQUAL_HEX8(0x00, joybus_crc8_update(0, 0x00));
  TEST_ASSERT_EQUAL_HEX8(0x85, joybus_crc8_update(0, 0x01));
  TEST_ASSERT_EQUAL_HEX8(0x89, joybus_crc8_update(0, 0x80));
}

// Test joybus_crc8 over an empty buffer
static void test_crc8_empty_buffer()
{
  TEST_ASSERT_EQUAL_HEX8(0x00, joybus_crc8(NULL, 0));
}

// Test joybus_crc8 over a single-byte buffer
static void test_crc8_single_byte()
{
  uint8_t buf = 0x80;
  TEST_ASSERT_EQUAL_HEX8(0x89, joybus_crc8(&buf, 1));
}

// Test joybus_crc8 over a multi-byte buffer
static void test_crc8_multi_byte()
{
  uint8_t buf[] = {0x80, 0x80};
  TEST_ASSERT_EQUAL_HEX8(0x36, joybus_crc8(buf, sizeof(buf)));
}

// Test address_checksum against various known values
static void test_address_checksum_known()
{
  TEST_ASSERT_EQUAL_HEX8(0x01, joybus_address_checksum(0x8000));
  TEST_ASSERT_EQUAL_HEX8(0x00, joybus_address_checksum(0x0000));
  TEST_ASSERT_EQUAL_HEX8(0x14, joybus_address_checksum(0x8020));
}

// Test address_checksum_valid against valid and invalid addresses
static void test_address_checksum_valid()
{
  TEST_ASSERT_TRUE(joybus_address_checksum_valid(0x8001));
  TEST_ASSERT_FALSE(joybus_address_checksum_valid(0x8000));
  TEST_ASSERT_TRUE(joybus_address_checksum_valid(0x0000));
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();

  RUN_TEST(test_crc8_update_known_bytes);
  RUN_TEST(test_crc8_empty_buffer);
  RUN_TEST(test_crc8_single_byte);
  RUN_TEST(test_crc8_multi_byte);

  RUN_TEST(test_address_checksum_known);
  RUN_TEST(test_address_checksum_valid);

  return UNITY_END();
}
