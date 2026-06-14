/**
 * @addtogroup joybus_host
 *
 * @{
 */

#pragma once

#include <joybus/bus.h>
#include <joybus/commands.h>
#include <joybus/identify.h>
#include <joybus/common/gcn_controller.h>

/**
 * Read the current input state of a GameCube controller.
 *
 * @param bus the Joybus instance to use
 * @param analog_mode the analog mode to use
 * @param motor_state the motor state to use
 * @param response buffer to store the input state response in
 * @return 0 on success, negative error code on failure
 */
int joybus_gcn_read(struct joybus *bus, enum joybus_gcn_analog_mode analog_mode,
                    enum joybus_gcn_motor_state motor_state, struct joybus_gcn_controller_state *response);

/**
 * Read the current input state of a GameCube controller, asynchronously.
 *
 * @param bus the Joybus instance to use
 * @param analog_mode the analog mode to use
 * @param motor_state the motor state to use
 * @param response buffer to store the input state response in
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_gcn_read_async(struct joybus *bus, enum joybus_gcn_analog_mode analog_mode,
                          enum joybus_gcn_motor_state motor_state, struct joybus_gcn_controller_state *response,
                          joybus_transfer_cb_t callback, void *user_data);

/**
 * Read the origin (neutral) state of a GameCube controller.
 *
 * @param bus the Joybus instance to use
 * @param response buffer to store the origin state response in
 * @return 0 on success, negative error code on failure
 */
int joybus_gcn_read_origin(struct joybus *bus, struct joybus_gcn_controller_state *response);

/**
 * Read the origin (neutral) state of a GameCube controller, asynchronously.
 *
 * @param bus the Joybus instance to use
 * @param response buffer to store the origin state response in
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_gcn_read_origin_async(struct joybus *bus, struct joybus_gcn_controller_state *response,
                                 joybus_transfer_cb_t callback, void *user_data);

/**
 * Calibrate a GameCube controller, setting its current input state as the origin.
 *
 * @param bus the Joybus instance to use
 * @param response buffer to store the origin state response in
 * @return 0 on success, negative error code on failure
 */
int joybus_gcn_calibrate(struct joybus *bus, struct joybus_gcn_controller_state *response);

/**
 * Calibrate a GameCube controller, setting its current input state as the origin, asynchronously.
 *
 * @param bus the Joybus instance to use
 * @param response buffer to store the origin state response
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_gcn_calibrate_async(struct joybus *bus, struct joybus_gcn_controller_state *response,
                               joybus_transfer_cb_t callback, void *user_data);

/**
 * Read the current input state of a GameCube controller, with full precision.
 *
 * @param bus the Joybus instance to use
 * @param motor_state the motor state to use (one of JOYBUS_GCN_MOTOR_*)
 * @param response buffer to store the input state response
 * @return 0 on success, negative error code on failure
 */
int joybus_gcn_read_long(struct joybus *bus, enum joybus_gcn_motor_state motor_state,
                         struct joybus_gcn_controller_state *response);

/**
 * Read the current input state of a GameCube controller, with full precision, asynchronously.
 *
 * @param bus the Joybus instance to use
 * @param motor_state the motor state to use (one of JOYBUS_GCN_MOTOR_*)
 * @param response buffer to store the input state response
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_gcn_read_long_async(struct joybus *bus, enum joybus_gcn_motor_state motor_state,
                               struct joybus_gcn_controller_state *response, joybus_transfer_cb_t callback,
                               void *user_data);

/**
 * Send a "probe device" command to a WaveBird controller.
 *
 * @param bus the Joybus instance to use
 * @param response buffer to store the response in
 * @return 0 on success, negative error code on failure
 */
int joybus_gcn_probe_device(struct joybus *bus, uint8_t response[JOYBUS_CMD_GCN_PROBE_DEVICE_RX]);

/**
 * Send a "probe device" command to a WaveBird controller, asynchronously.
 *
 * @param bus the Joybus instance to use
 * @param response buffer to store the response in
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 */
int joybus_gcn_probe_device_async(struct joybus *bus, uint8_t response[JOYBUS_CMD_GCN_PROBE_DEVICE_RX],
                                  joybus_transfer_cb_t callback, void *user_data);

/**
 * Send a "fix device" command to a WaveBird controller.
 *
 * Non-wireless controllers will ignore this command. If successful, the response
 * buffer will contain the updated controller ID.
 *
 * @param bus the Joybus instance to use
 * @param wireless_id the 10-bit wireless ID to fix the controller to
 * @param response buffer to store the identity response in
 */
int joybus_gcn_fix_device(struct joybus *bus, uint16_t wireless_id, struct joybus_id *response);

/**
 * Send a "fix device" command to a WaveBird controller, asynchronously.
 *
 * Non-wireless controllers will ignore this command. If successful, the response
 * buffer will contain the updated controller ID.
 *
 * @param bus the Joybus instance to use
 * @param wireless_id the 10-bit wireless ID to fix the controller to
 * @param response buffer to store the identity response in
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_gcn_fix_device_async(struct joybus *bus, uint16_t wireless_id, struct joybus_id *response,
                                joybus_transfer_cb_t callback, void *user_data);

/** @} */
