/**
 * Common Joybus commands.
 *
 * @addtogroup joybus_host
 * @{
 */

#pragma once

#include <joybus/bus.h>

/**
 * Identify the target device attached to the Joybus.
 *
 * Calls the provided callback when complete. The response buffer will be populated with
 * a 3-byte response containing the device ID and status.
 *
 * @param bus the Joybus to use
 * @param response buffer to store the response in
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_identify(struct joybus *bus, uint8_t *response, joybus_transfer_cb_t callback, void *user_data);

/**
 * Reset the target device attached to the Joybus.
 *
 * @param bus the Joybus to use
 * @param response buffer to store the response in
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_reset(struct joybus *bus, uint8_t *response, joybus_transfer_cb_t callback, void *user_data);

/** @} */