#pragma once

#include <joybus/bus.h>

/**
 * Initialize a rumble pak.
 *
 * @param bus the bus with a controller with a rumble pak attached
 * @return 0 on success, negative joybus_error on failure
 */
int joybus_n64_rumble_pak_init(struct joybus *bus);

/**
 * Initialize a rumble pak, asynchronously.
 *
 * @param bus the bus with a controller with a rumble pak attached
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative joybus_error on failure
 */
int joybus_n64_rumble_pak_init_async(struct joybus *bus, joybus_transfer_cb callback, void *user_data);

/**
 * Start the motor on a rumble pak.
 *
 * @param bus the bus with a controller with a rumble pak attached
 */
int joybus_n64_rumble_pak_start(struct joybus *bus);

/**
 * Start the motor on a rumble pak, asynchronously.
 *
 * @param bus the bus with a controller with a rumble pak attached
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative joybus_error on failure
 */
int joybus_n64_rumble_pak_start_async(struct joybus *bus, joybus_transfer_cb callback, void *user_data);

/**
 * Stop the motor on a rumble pak.
 *
 * @param bus the bus with a controller with a rumble pak attached
 */
int joybus_n64_rumble_pak_stop(struct joybus *bus);

/**
 * Stop the motor on a rumble pak, asynchronously.
 *
 * @param bus the bus with a controller with a rumble pak attached
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative joybus_error on failure
 */
int joybus_n64_rumble_pak_stop_async(struct joybus *bus, joybus_transfer_cb callback, void *user_data);
