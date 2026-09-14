#include <string.h>

#include <joybus/commands.h>
#include <joybus/errors.h>
#include <joybus/target.h>
#include <joybus/target/pixelfx_gameid.h>

#include "unity.h"

#include "harness.h"

// The game ID target under test
static struct joybus_target_pixelfx_gameid gameid;

// Spy for the received callback
static int received_count;
static void on_received(struct joybus_target_pixelfx_gameid *target)
{
  received_count++;
}

void setUp(void)
{
  joybus_target_pixelfx_gameid_init(&gameid);
  joybus_target_pixelfx_gameid_set_received_cb(&gameid, on_received);
  harness_reset(JOYBUS_TARGET(&gameid));
  received_count = 0;
}

void tearDown(void)
{
}

// Test that a game ID command stores the ID, fires the callback, and sends no reply
static void test_gameid_received(void)
{
  // F-ZERO X, from the PixelFX README
  uint8_t command[] = {JOYBUS_CMD_PIXELFX_GAMEID, 0xB3, 0x0E, 0xD9, 0x78, 0x30, 0x03, 0xC9, 0xF9, 0x43, 0x45};
  TEST_ASSERT_EQUAL(0, send_command(command, sizeof(command)));

  TEST_ASSERT_EQUAL_HEX8_ARRAY(&command[1], gameid.game_id, JOYBUS_PIXELFX_GAMEID_SIZE);
  TEST_ASSERT_EQUAL(1, received_count);
  TEST_ASSERT_EQUAL(0, response.count);
}

// Test that any other command is declined on its first byte
static void test_other_command_declined(void)
{
  uint8_t command[] = {JOYBUS_CMD_IDENTIFY};
  TEST_ASSERT_EQUAL(-JOYBUS_ERR_NOT_SUPPORTED, send_command(command, sizeof(command)));

  TEST_ASSERT_EQUAL(0, received_count);
  TEST_ASSERT_EQUAL(0, response.count);
}

int main(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_gameid_received);
  RUN_TEST(test_other_command_declined);

  return UNITY_END();
}
