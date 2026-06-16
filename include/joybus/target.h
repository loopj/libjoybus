/**
 * @addtogroup joybus_target
 *
 * A Joybus target is a device on the bus that responds to commands from a host
 * (the console), such as an N64 or GameCube controller.
 *
 * Commands arrive one byte at a time. As each byte is received, libjoybus
 * calls the ::joybus_target_api::byte_received handler with the bytes
 * accumulated so far. The handler inspects them to decide how many more bytes
 * the command needs, returning the count of bytes still expected.
 *
 * To send a response, the handler must call the "response ready" callback with
 * a pointer to the (long-lived) response data and its length.
 *
 * Handlers should call the response callback as soon as the response is ready,
 * even if they are still expecting more command bytes. For many commands, the
 * response data is fully determined by the first few bytes of the command. This
 * allows the backend to start transmitting the response *immediately* after
 * the last byte is received.
 *
 * To create your own target, define a struct whose first member is a
 * ::joybus_target (so it can be cast through ::JOYBUS_TARGET), point its api
 * at a ::joybus_target_api table, and register it on a bus with
 * joybus_target_register().
 *
 * @{
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

struct joybus_target;

/// Macro to cast a concrete Joybus target instance to a generic Joybus target instance.
#define JOYBUS_TARGET(target) ((struct joybus_target *)(target))

/**
 * Callback type for sending responses from target command handlers.
 *
 * @param response the response data to send
 * @param len the length of the response data
 * @param user_data user data passed to the command handler
 */
typedef void (*joybus_target_response_cb)(const uint8_t *response, uint8_t len, void *user_data);

/**
 * API for implementing a Joybus target.
 */
struct joybus_target_api {
  /**
   * Handle a received command byte.
   *
   * @param target the target to handle the command
   * @param command the command buffer
   * @param byte_idx the index of the byte that was just received
   * @param send_response a callback function to send the response
   * @param user_data user data to pass to the response callback
   * @return positive number of bytes still expected, 0 if no more bytes expected, a negative joybus_error on failure
   */
  int (*byte_received)(struct joybus_target *target, const uint8_t *command, uint8_t byte_idx,
                       joybus_target_response_cb send_response, void *user_data);
};

/**
 * Interface for a Joybus target, a device on the Joybus which responds to commands from a host.
 */
struct joybus_target {
  /// API for handling received commands
  const struct joybus_target_api *api;

  /// Whether the target is currently registered on the bus
  bool registered;
};

/**
 * Handle a received command byte for a Joybus target.
 *
 * @param target the target to handle the command
 * @param command the command buffer
 * @param byte_idx the index of the byte that was just received
 * @param send_response a callback function to send the response
 * @param user_data user data to pass to the response callback
 * @return positive number of bytes still expected, 0 if no more bytes expected, a negative joybus_error on failure
 */
static inline int joybus_target_byte_received(struct joybus_target *target, const uint8_t *command, uint8_t byte_idx,
                                              joybus_target_response_cb send_response, void *user_data)
{
  return target->api->byte_received(target, command, byte_idx, send_response, user_data);
}

/**
 * Check if a target is currently registered on the bus.
 *
 * @param target the target to check
 * @return true if the target is registered, false otherwise
 */
static inline bool joybus_target_is_registered(struct joybus_target *target)
{
  return target->registered;
}

/** @} */
