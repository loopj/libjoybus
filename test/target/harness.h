#pragma once

#include <string.h>

#include <joybus/bus.h>
#include <joybus/target.h>

#include "unity.h"

// The target under test, set by harness_reset()
static struct joybus_target *target_under_test;

// Monotonic event counter for asserting relative ordering.
static int event_seq;

// The last response sent by the target
static struct {
  uint8_t data[JOYBUS_BLOCK_SIZE]; ///< Response bytes
  uint8_t len;                     ///< Response length
  int count;                       ///< Responses sent since harness_reset()
  uint8_t at_byte;                 ///< 1-based command byte that triggered the response
  int seq;                         ///< event_seq stamp taken when the response was sent
} response;

// Command byte currently being delivered, 1-based
static uint8_t current_byte;

// joybus_target_response_cb_t that records the target's response
static void record_response(const uint8_t *data, uint8_t len, void *user_data)
{
  TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(sizeof(response.data), len, "response larger than JOYBUS_BLOCK_SIZE");
  memcpy(response.data, data, len);
  response.len     = len;
  response.at_byte = current_byte;
  response.seq     = ++event_seq;
  response.count++;
}

// Deliver a complete command to the target byte-by-byte
static inline int send_command(const uint8_t *command, uint8_t len)
{
  for (uint8_t i = 1; i <= len; i++) {
    // Keep track of the current byte index for error reporting
    current_byte  = i;

    // Call the target's byte-received handler and check the result
    int remaining = joybus_target_byte_received(target_under_test, command, i, record_response, NULL);
    if (remaining < 0)
      return remaining;

    // Check that the handler reports the correct number of bytes remaining
    TEST_ASSERT_EQUAL_MESSAGE(len - i, remaining, "bytes-remaining contract violated");
  }

  return 0;
}

// Point the harness at a target and clear all recorded state; call from setUp()
static inline void harness_reset(struct joybus_target *target)
{
  target_under_test = target;
  memset(&response, 0, sizeof(response));
  current_byte = 0;
  event_seq    = 0;
}
