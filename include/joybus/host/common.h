/**
 * @addtogroup joybus_host
 *
 * @{
 */

#pragma once

#include <joybus/bus.h>
#include <joybus/identify.h>

/**
 * Identify the target device attached to the Joybus.
 *
 * @param bus the Joybus to use
 * @param response buffer to store the identify response in
 * @return 0 on success, a negative joybus_error on failure
 */
int joybus_identify(struct joybus *bus, struct joybus_id *response);

/**
 * Identify the target device attached to the Joybus, asynchronously.
 *
 * Calls the provided callback when complete. The response buffer will be populated with
 * a 3-byte response containing the device ID and status.
 *
 * @param bus the Joybus to use
 * @param response buffer to store the identify response in
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 if the transfer was started, a negative joybus_error otherwise
 */
int joybus_identify_async(struct joybus *bus, struct joybus_id *response, joybus_transfer_cb callback,
                          void *user_data);

/**
 * Reset the target device attached to the Joybus.
 *
 * @param bus the Joybus to use
 * @param response buffer to store the reset response in
 * @return 0 on success, a negative joybus_error on failure
 */
int joybus_reset(struct joybus *bus, struct joybus_id *response);

/**
 * Reset the target device attached to the Joybus, asynchronously.
 *
 * @param bus the Joybus instance to use
 * @param response buffer to store the identify response in
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 if the transfer was started, a negative joybus_error otherwise
 */
int joybus_reset_async(struct joybus *bus, struct joybus_id *response, joybus_transfer_cb callback, void *user_data);

/** @} */
