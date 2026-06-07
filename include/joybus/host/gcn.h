/**
 * @addtogroup joybus_host
 *
 * @{
 */

#pragma once

#include <joybus/bus.h>
#include <joybus/common/gcn_controller.h>

/**
 * Read the current input state of a GameCube controller.
 *
 * @param bus the Joybus to use
 * @param analog_mode the analog mode to use (one of JOYBUS_GCN_ANALOG_MODE_*)
 * @param motor_state the motor state to use (one of JOYBUS_GCN_MOTOR_*)
 * @param response buffer to store the response in, must be at least JOYBUS_CMD_GCN_READ_RX bytes
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_gcn_read(struct joybus *bus, enum joybus_gcn_analog_mode analog_mode,
                    enum joybus_gcn_motor_state motor_state, uint8_t *response, joybus_transfer_cb_t callback,
                    void *user_data);

/**
 * Read the origin (neutral) state of a GameCube controller.
 *
 * @param bus the Joybus to use
 * @param response buffer to store the response in, must be at least JOYBUS_CMD_GCN_READ_ORIGIN_RX bytes
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_gcn_read_origin(struct joybus *bus, uint8_t *response, joybus_transfer_cb_t callback, void *user_data);

/**
 * Calibrate a GameCube controller, setting its current input state as the origin.
 *
 * @param bus the Joybus to use
 * @param response buffer to store the response in, must be at least JOYBUS_CMD_GCN_CALIBRATE_RX bytes
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_gcn_calibrate(struct joybus *bus, uint8_t *response, joybus_transfer_cb_t callback, void *user_data);

/**
 * Read the current input state of a GameCube controller, with full precision.
 *
 * @param bus the Joybus to use
 * @param motor_state the motor state to use (one of JOYBUS_GCN_MOTOR_*)
 * @param response buffer to store the response in, must be at least JOYBUS_CMD_GCN_READ_LONG_RX bytes
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_gcn_read_long(struct joybus *bus, enum joybus_gcn_motor_state motor_state, uint8_t *response,
                         joybus_transfer_cb_t callback, void *user_data);

/**
 * Send a "probe device" command to a WaveBird controller.
 *
 * Non-wireless controllers will ignore this command.
 *
 * @param bus the Joybus to use
 * @param response buffer to store the response in, must be at least JOYBUS_CMD_GCN_PROBE_DEVICE_RX bytes
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 */
int joybus_gcn_probe_device(struct joybus *bus, uint8_t *response, joybus_transfer_cb_t callback, void *user_data);

/**
 * Send a "fix device" command to a WaveBird controller.
 *
 * Non-wireless controllers will ignore this command. If successful, the response
 * buffer will contain the updated controller ID.
 *
 * @param bus the Joybus to use
 * @param wireless_id the 10-bit wireless ID to fix the controller to
 * @param response buffer to store the response in, must be at least JOYBUS_CMD_GCN_FIX_DEVICE_RX bytes
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_gcn_fix_device(struct joybus *bus, uint16_t wireless_id, uint8_t *response, joybus_transfer_cb_t callback,
                          void *user_data);

/**
 * Helper function to unpack raw input data from a GameCube controller.
 *
 * The raw response from a "read" command packs the controller's input state into
 * 8 bytes according to the analog mode. See enum joybus_gcn_analog_mode for more
 * details.
 *
 * @param dest pointer to a joybus_gcn_controller_input struct to store the unpacked data
 * @param src pointer to the response buffer from a "read" command
 * @param analog_mode the analog mode used when reading (one of JOYBUS_GCN_ANALOG_MODE_*)
 */
int joybus_gcn_unpack_input(struct joybus_gcn_controller_input *dest, const uint8_t *src,
                            enum joybus_gcn_analog_mode analog_mode);

/** @} */