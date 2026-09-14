#include <string.h>

#include <joybus/bus.h>
#include <joybus/errors.h>
#include <joybus/target.h>

#include "unity.h"

// A fake target that accepts a single opcode and records every byte it is offered
struct fake_target {
  struct joybus_target base;
  uint8_t opcode;         ///< The one opcode this target accepts
  uint8_t len;            ///< Length of a command with that opcode
  uint8_t refuse_at;      ///< Byte index at which to refuse a claimed command, 0 to never refuse
  int calls;              ///< Number of byte_received calls
  uint8_t offered[8];     ///< Byte index passed to each call
  uint8_t response[2];    ///< Response sent on the first byte of an accepted command
};

static int fake_byte_received(struct joybus_target *target, const uint8_t *command, uint8_t byte_idx,
                              joybus_target_response_cb send_response, void *user_data)
{
  struct fake_target *fake = (struct fake_target *)target;

  fake->offered[fake->calls] = byte_idx;
  fake->calls++;

  // Decline commands with any other opcode
  if (byte_idx == 1 && command[0] != fake->opcode)
    return -JOYBUS_ERR_NOT_SUPPORTED;

  if (byte_idx == fake->refuse_at)
    return -JOYBUS_ERR_NOT_SUPPORTED;

  if (byte_idx == 1)
    send_response(fake->response, sizeof(fake->response), user_data);

  return fake->len - byte_idx;
}

static const struct joybus_target_api fake_api = {
  .byte_received = fake_byte_received,
};

// The bus and targets under test
static struct joybus bus;
static struct fake_target first;
static struct fake_target second;

// The last response sent by any target
static const uint8_t *response_data;
static int response_count;

static void record_response(const uint8_t *data, uint8_t len, void *user_data)
{
  response_data = data;
  response_count++;
}

static void fake_init(struct fake_target *fake, uint8_t opcode, uint8_t len)
{
  memset(fake, 0, sizeof(*fake));
  fake->base.api = &fake_api;
  fake->opcode   = opcode;
  fake->len      = len;
}

// Deliver one byte of a command to the bus
static int feed_byte(const uint8_t *command, uint8_t byte_idx)
{
  return joybus_byte_received(&bus, command, byte_idx, record_response, NULL);
}

void setUp(void)
{
  memset(&bus, 0, sizeof(bus));
  fake_init(&first, 0x01, 1);
  fake_init(&second, 0x03, 3);
  response_data  = NULL;
  response_count = 0;
}

void tearDown(void)
{
}

static void test_attach_keeps_attachment_order(void)
{
  joybus_attach_target(&bus, JOYBUS_TARGET(&first));
  joybus_attach_target(&bus, JOYBUS_TARGET(&second));

  TEST_ASSERT_EQUAL_PTR(&first, bus.targets);
  TEST_ASSERT_EQUAL_PTR(&second, first.base.next);
  TEST_ASSERT_NULL(second.base.next);
  TEST_ASSERT_TRUE(joybus_target_is_attached(JOYBUS_TARGET(&first)));
  TEST_ASSERT_TRUE(joybus_target_is_attached(JOYBUS_TARGET(&second)));
}

static void test_attach_twice_has_no_effect(void)
{
  joybus_attach_target(&bus, JOYBUS_TARGET(&first));
  joybus_attach_target(&bus, JOYBUS_TARGET(&first));

  TEST_ASSERT_EQUAL_PTR(&first, bus.targets);
  TEST_ASSERT_NULL(first.base.next);
}

static void test_detach_unlinks_target(void)
{
  joybus_attach_target(&bus, JOYBUS_TARGET(&first));
  joybus_attach_target(&bus, JOYBUS_TARGET(&second));

  // Detach the head, the second target moves up
  joybus_detach_target(&bus, JOYBUS_TARGET(&first));
  TEST_ASSERT_EQUAL_PTR(&second, bus.targets);
  TEST_ASSERT_FALSE(joybus_target_is_attached(JOYBUS_TARGET(&first)));
  TEST_ASSERT_TRUE(joybus_target_is_attached(JOYBUS_TARGET(&second)));

  // Detach the last one, the bus has no targets
  joybus_detach_target(&bus, JOYBUS_TARGET(&second));
  TEST_ASSERT_NULL(bus.targets);
  TEST_ASSERT_FALSE(joybus_target_is_attached(JOYBUS_TARGET(&second)));
}

static void test_detach_tail_keeps_head(void)
{
  joybus_attach_target(&bus, JOYBUS_TARGET(&first));
  joybus_attach_target(&bus, JOYBUS_TARGET(&second));

  joybus_detach_target(&bus, JOYBUS_TARGET(&second));
  TEST_ASSERT_EQUAL_PTR(&first, bus.targets);
  TEST_ASSERT_NULL(first.base.next);
  TEST_ASSERT_TRUE(joybus_target_is_attached(JOYBUS_TARGET(&first)));
}

static void test_no_targets_is_unsupported(void)
{
  const uint8_t command[] = {0x01};
  TEST_ASSERT_EQUAL(-JOYBUS_ERR_NOT_SUPPORTED, feed_byte(command, 1));
}

static void test_first_attached_wins_shared_opcode(void)
{
  // Both targets accept the same opcode
  fake_init(&second, 0x01, 1);
  joybus_attach_target(&bus, JOYBUS_TARGET(&first));
  joybus_attach_target(&bus, JOYBUS_TARGET(&second));

  const uint8_t command[] = {0x01};
  TEST_ASSERT_EQUAL(0, feed_byte(command, 1));

  TEST_ASSERT_EQUAL(1, first.calls);
  TEST_ASSERT_EQUAL(0, second.calls);
  TEST_ASSERT_EQUAL(1, response_count);
  TEST_ASSERT_EQUAL_PTR(first.response, response_data);
  TEST_ASSERT_EQUAL_PTR(&first, bus.active_target);
}

static void test_declined_command_reaches_second(void)
{
  joybus_attach_target(&bus, JOYBUS_TARGET(&first));
  joybus_attach_target(&bus, JOYBUS_TARGET(&second));

  const uint8_t command[] = {0x03, 0x11, 0x22};
  TEST_ASSERT_EQUAL(2, feed_byte(command, 1));

  // The first target was offered the byte and declined, the second claimed it
  TEST_ASSERT_EQUAL(1, first.calls);
  TEST_ASSERT_EQUAL(1, second.calls);
  TEST_ASSERT_EQUAL(1, response_count);
  TEST_ASSERT_EQUAL_PTR(second.response, response_data);
  TEST_ASSERT_EQUAL_PTR(&second, bus.active_target);
}

static void test_later_bytes_go_to_active_target_only(void)
{
  joybus_attach_target(&bus, JOYBUS_TARGET(&first));
  joybus_attach_target(&bus, JOYBUS_TARGET(&second));

  const uint8_t command[] = {0x03, 0x11, 0x22};
  TEST_ASSERT_EQUAL(2, feed_byte(command, 1));
  TEST_ASSERT_EQUAL(1, feed_byte(command, 2));
  TEST_ASSERT_EQUAL(0, feed_byte(command, 3));

  // The first target only ever saw the first byte
  TEST_ASSERT_EQUAL(1, first.calls);
  TEST_ASSERT_EQUAL(1, first.offered[0]);

  // The second target saw every byte, in order
  TEST_ASSERT_EQUAL(3, second.calls);
  TEST_ASSERT_EQUAL(1, second.offered[0]);
  TEST_ASSERT_EQUAL(2, second.offered[1]);
  TEST_ASSERT_EQUAL(3, second.offered[2]);
  TEST_ASSERT_EQUAL(1, response_count);
}

static void test_unclaimed_command_is_unsupported(void)
{
  joybus_attach_target(&bus, JOYBUS_TARGET(&first));
  joybus_attach_target(&bus, JOYBUS_TARGET(&second));

  const uint8_t command[] = {0x40};
  TEST_ASSERT_EQUAL(-JOYBUS_ERR_NOT_SUPPORTED, feed_byte(command, 1));

  // Both targets were offered the byte, nothing was sent
  TEST_ASSERT_EQUAL(1, first.calls);
  TEST_ASSERT_EQUAL(1, second.calls);
  TEST_ASSERT_EQUAL(0, response_count);
  TEST_ASSERT_NULL(bus.active_target);
}

static void test_active_target_refusing_later_byte_is_error(void)
{
  second.refuse_at = 2;
  joybus_attach_target(&bus, JOYBUS_TARGET(&first));
  joybus_attach_target(&bus, JOYBUS_TARGET(&second));

  const uint8_t command[] = {0x03, 0x11, 0x22};
  TEST_ASSERT_EQUAL(2, feed_byte(command, 1));
  TEST_ASSERT_EQUAL(-JOYBUS_ERR_NOT_SUPPORTED, feed_byte(command, 2));

  // The refused byte was not offered to the first target
  TEST_ASSERT_EQUAL(1, first.calls);
  TEST_ASSERT_EQUAL(2, second.calls);
}

static void test_abandoned_command_does_not_affect_next(void)
{
  joybus_attach_target(&bus, JOYBUS_TARGET(&first));
  joybus_attach_target(&bus, JOYBUS_TARGET(&second));

  // The host stops after two bytes of a three byte command claimed by the second target
  const uint8_t abandoned[] = {0x03, 0x11, 0x22};
  TEST_ASSERT_EQUAL(2, feed_byte(abandoned, 1));
  TEST_ASSERT_EQUAL(1, feed_byte(abandoned, 2));

  // The next command is offered from the start and claimed by the first target
  const uint8_t next[] = {0x01};
  TEST_ASSERT_EQUAL(0, feed_byte(next, 1));
  TEST_ASSERT_EQUAL(2, first.calls);
  TEST_ASSERT_EQUAL(2, second.calls);
  TEST_ASSERT_EQUAL_PTR(first.response, response_data);
  TEST_ASSERT_EQUAL_PTR(&first, bus.active_target);

  // An unclaimed command after an abandoned one is still unsupported
  const uint8_t unknown[] = {0x40};
  TEST_ASSERT_EQUAL(-JOYBUS_ERR_NOT_SUPPORTED, feed_byte(unknown, 1));
  TEST_ASSERT_NULL(bus.active_target);
}

static void test_detaching_active_target_ends_command(void)
{
  joybus_attach_target(&bus, JOYBUS_TARGET(&first));
  joybus_attach_target(&bus, JOYBUS_TARGET(&second));

  const uint8_t command[] = {0x03, 0x11, 0x22};
  TEST_ASSERT_EQUAL(2, feed_byte(command, 1));

  joybus_detach_target(&bus, JOYBUS_TARGET(&second));
  TEST_ASSERT_EQUAL(-JOYBUS_ERR_NOT_SUPPORTED, feed_byte(command, 2));

  // The detached target saw nothing more, and the first target was not offered a later byte
  TEST_ASSERT_EQUAL(1, first.calls);
  TEST_ASSERT_EQUAL(1, second.calls);
}

int main(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_attach_keeps_attachment_order);
  RUN_TEST(test_attach_twice_has_no_effect);
  RUN_TEST(test_detach_unlinks_target);
  RUN_TEST(test_detach_tail_keeps_head);

  RUN_TEST(test_no_targets_is_unsupported);
  RUN_TEST(test_first_attached_wins_shared_opcode);
  RUN_TEST(test_declined_command_reaches_second);
  RUN_TEST(test_later_bytes_go_to_active_target_only);
  RUN_TEST(test_unclaimed_command_is_unsupported);
  RUN_TEST(test_active_target_refusing_later_byte_is_error);
  RUN_TEST(test_abandoned_command_does_not_affect_next);
  RUN_TEST(test_detaching_active_target_ends_command);

  return UNITY_END();
}
