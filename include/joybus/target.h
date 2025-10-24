/**
 * @addtogroup joybus_target
 * @{
 */

#pragma once

#include <stdint.h>

struct joybus_target;

/**
 * Macro to cast a concrete Joybus target instance to a generic Joybus target instance.
 */
#define JOYBUS_TARGET(target) ((struct joybus_target *)(target))

/**
 * Callback type for sending responses from target command handlers.
 *
 * @param response the response data to send
 * @param len the length of the response data
 * @param user_data user data passed to the command handler
 */
typedef void (*joybus_target_response_cb_t)(const uint8_t *response, uint8_t len, void *user_data);

// API for a Joybus target.
struct joybus_target_api {
  /**
   * Handle a received command byte.
   *
   * @param target the target to handle the command
   * @param command the command buffer
   * @param byte_idx the index of the byte that was just received
   * @param send_response a callback function to send the response
   * @param user_data user data to pass to the response callback
   * @return positive number of bytes still expected, 0 if no more bytes expected, negative error code on failure
   */
  int (*byte_received)(struct joybus_target *target, const uint8_t *command, uint8_t byte_idx,
                       joybus_target_response_cb_t send_response, void *user_data);
};

/**
 * A Joybus target, a device on the Joybus that can respond to commands.
 */
struct joybus_target {
  const struct joybus_target_api *api;
};

/**
 * Handle a received command byte for a Joybus target.
 *
 * @param target the target to handle the command
 * @param command the command buffer
 * @param byte_idx the index of the byte that was just received
 * @param send_response a callback function to send the response
 * @param user_data user data to pass to the response callback
 * @return positive number of bytes still expected, 0 if no more bytes expected, negative error code on failure
 */
static inline int joybus_target_byte_received(struct joybus_target *target, const uint8_t *command, uint8_t byte_idx,
                                              joybus_target_response_cb_t send_response, void *user_data)
{
  return target->api->byte_received(target, command, byte_idx, send_response, user_data);
}

/** @} */