/**
 * @defgroup joybus_target_gcn_controller GameCube Controller Target
 * @ingroup joybus_target
 *
 * Joybus target implementation for standard GameCube controllers and WaveBird receivers.
 *
 * @{
 */

#pragma once

#include <stdbool.h>

#include <joybus/identify.h>
#include <joybus/target.h>
#include <joybus/common/gcn_controller.h>

struct joybus_gcn_controller;

/// Macro to cast from a generic Joybus target to a GameCube controller target
#define JOYBUS_GCN_CONTROLLER(target) ((struct joybus_gcn_controller *)(target))

/**
 * Callback type for GameCube controller reset events.
 *
 * @param controller the controller that was reset
 */
typedef void (*joybus_gcn_controller_reset_cb_t)(struct joybus_gcn_controller *controller);

/**
 * Callback type for GameCube controller motor state change events.
 *
 * @param controller the controller whose motor state changed
 * @param state the new motor state
 */
typedef void (*joybus_gcn_controller_motor_cb_t)(struct joybus_gcn_controller *controller, uint8_t state);

/**
 * GameCube controller Joybus target.
 */
struct joybus_gcn_controller {
  /// Base target interface
  struct joybus_target base;

  /// Controller ID
  uint8_t id[3];

  /// Origin input state
  struct joybus_gcn_controller_input origin;

  /// Current input state
  struct joybus_gcn_controller_input input;

  /// Packed input state buffer
  uint8_t packed_input[8];

  /// Whether the input state is valid
  bool input_valid;

  /// Callback for controller reset events
  joybus_gcn_controller_reset_cb_t on_reset;

  /// Callback for controller motor state change events
  joybus_gcn_controller_motor_cb_t on_motor_state_change;
};

/**
 * Initialize a GameCube controller target with a specific controller type.
 *
 * @param controller the controller to initialize
 * @param type the controller type flags
 */
void joybus_gcn_controller_init_with_type(struct joybus_gcn_controller *controller, uint16_t type);

/**
 * Initialize a GameCube controller target as a regular wired controller.
 *
 * @param controller the controller to initialize
 */
static inline void joybus_gcn_controller_init(struct joybus_gcn_controller *controller)
{
  joybus_gcn_controller_init_with_type(controller, JOYBUS_DEVICE_GCN_CONTROLLER);
}

/**
 * Initialize a GameCube controller target as a WaveBird receiver.
 *
 * @param controller the controller to initialize
 */
static inline void joybus_gcn_controller_init_wavebird(struct joybus_gcn_controller *controller)
{
  joybus_gcn_controller_init_with_type(controller, JOYBUS_DEVICE_GCN_WAVEBIRD);
}

/**
 * Set the reset callback for the controller.
 *
 * NOTE: Reset callbacks are called from interrupt context, do not perform any
 *       blocking operations within the callback.
 *
 * @param controller the controller to set the callback for
 * @param callback the callback function
 */
void joybus_gcn_controller_set_reset_callback(struct joybus_gcn_controller *controller,
                                              joybus_gcn_controller_reset_cb_t callback);

/**
 * Set the motor state change callback for the controller.
 *
 * NOTE: Motor state callbacks are called from interrupt context, do not
 *       perform any blocking operations within the callback.
 *
 * @param controller the controller to set the callback for
 * @param callback the callback function
 */
void joybus_gcn_controller_set_motor_callback(struct joybus_gcn_controller *controller,
                                              joybus_gcn_controller_motor_cb_t callback);

/**
 * Check if the controller is a WaveBird controller.
 *
 * @param controller the controller to check
 * @return true if the controller is a WaveBird controller
 */
static inline bool joybus_gcn_controller_is_wireless(struct joybus_gcn_controller *controller)
{
  return joybus_id_get_type(controller->id) & JOYBUS_TYPE_GCN_WIRELESS;
}

/**
 * Set the wireless ID of the controller.
 *
 * Wireless IDs are 10-bit numbers used to identify a WaveBird controller.
 * Although these IDs aren’t globally unique, they are assumed to be distinct enough
 * so that it’s unlikely for a single user to have two controllers with the same ID.
 * The ID helps bind a controller to a specific port after data reception.
 *
 * @param controller the controller to set the wireless ID for
 * @param wireless_id the new 10-bit wireless ID
 */
void joybus_gcn_controller_set_wireless_id(struct joybus_gcn_controller *controller, uint16_t wireless_id);

/**
 * Get the current wireless ID of the controller.
 *
 * @param controller the controller to get the wireless ID from
 * @return the current 10-bit wireless ID
 */
static inline uint16_t joybus_gcn_controller_get_wireless_id(struct joybus_gcn_controller *controller)
{
  return (controller->id[1] & 0xC0) << 2 | controller->id[2];
}

/**
 * Determine if the wireless ID has been fixed.
 *
 * Fixing the wireless ID is used to bind a WaveBird controller to a specific receiver.
 *
 * @param controller the controller to check
 * @return true if the wireless ID is fixed
 */
static inline bool joybus_gcn_controller_wireless_id_fixed(struct joybus_gcn_controller *controller)
{
  return joybus_id_get_type(controller->id) & JOYBUS_TYPE_GCN_WIRELESS_ID_FIXED;
}

/**
 * Mark the input state as valid.
 *
 * When the input state is marked valid, we'll use the contents of controller->input
 * when replying to poll commands, otherwise we'll use the origin state.
 *
 * @param controller the controller to set the input state for
 * @param valid true if the input state is valid
 */
static inline void joybus_gcn_controller_input_valid(struct joybus_gcn_controller *controller, bool valid)
{
  controller->input_valid = valid;
}

/**
 * Update the origin of the controller.
 *
 * If the origin data differs from the current origin, the "need origin" flag is set.
 *
 * @param controller the controller to set the wireless origin for
 * @param new_origin pointer to the new origin data (6 bytes)
 */
void joybus_gcn_controller_set_origin(struct joybus_gcn_controller *controller,
                                      struct joybus_gcn_controller_input *new_origin);

/** @} */
