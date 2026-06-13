/**
 * @addtogroup joybus_host
 *
 * @{
 */

#pragma once

#include <joybus/bus.h>
#include <joybus/commands.h>
#include <joybus/common/n64_controller.h>

/**
 * Read the current input state of an N64 controller or mouse.
 *
 * @param bus the Joybus instance to use
 * @param response buffer to store the response in
 * @return 0 on success, negative error code on failure
 */
int joybus_n64_read(struct joybus *bus, struct joybus_n64_controller_state *response);

/**
 * Read the current input state of an N64 controller or mouse, asynchronously.
 *
 * @param bus the Joybus instance to use
 * @param response buffer to store the response in
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_n64_read_async(struct joybus *bus, struct joybus_n64_controller_state *response,
                          joybus_transfer_cb_t callback, void *user_data);

/**
 * Write a block of data to the pak attached to an N64 controller, asynchronously.
 *
 * This writes a raw block to the pak. Most callers should use a higher-level pak
 * function (such as the Rumble Pak helpers) instead. Use this directly only when
 * implementing one.
 *
 * @param bus the Joybus instance to use
 * @param addr the address to write to, must be 32-byte aligned
 * @param data the data to write
 * @param response buffer to store the response in
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_n64_pak_write_async(struct joybus *bus, uint16_t addr, const uint8_t data[JOYBUS_PAK_BLOCK_SIZE],
                               uint8_t response[JOYBUS_CMD_N64_PAK_WRITE_RX], joybus_transfer_cb_t callback,
                               void *user_data);

/**
 * Read a block of data from the pak attached to an N64 controller, asynchronously.
 *
 * This reads a raw block from the pak. Most callers should use a higher-level
 * pak function (such as the Rumble Pak helpers) instead. Use this directly only
 * when implementing one.
 *
 * @param bus the Joybus to use
 * @param addr the address to read from, must be 32-byte aligned
 * @param response buffer to store the response in, must be at least JOYBUS_CMD_N64_PAK_READ_RX bytes
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_n64_pak_read_async(struct joybus *bus, uint16_t addr, uint8_t response[JOYBUS_CMD_N64_PAK_READ_RX],
                              joybus_transfer_cb_t callback, void *user_data);

/** @} */
