/**
 * @defgroup joybus_target_gc_controller GameCube Controller Target
 * GameCube controller Joybus target
 * @ingroup joybus_target
 * @{
 */

#pragma once

#include <stdbool.h>

#include <joybus/gamecube.h>
#include <joybus/target.h>

struct joybus_gc_controller;

/** Macro to cast to a GameCube controller target */
#define JOYBUS_GC_CONTROLLER(target) ((struct joybus_gc_controller *)(target))

/**
 * Callback type for GameCube controller reset events.
 *
 * @param controller the controller that was reset
 */
typedef void (*joybus_gc_controller_reset_cb_t)(struct joybus_gc_controller *controller);

/**
 * Callback type for GameCube controller motor state change events.
 *
 * @param controller the controller whose motor state changed
 * @param state the new motor state
 */
typedef void (*joybus_gc_controller_motor_cb_t)(struct joybus_gc_controller *controller, uint8_t state);

/**
 * GameCube controller Joybus target.
 */
struct joybus_gc_controller {
  struct joybus_target base;

  /**< Controller ID */
  uint8_t id[3];

  /** Origin input state */
  struct joybus_gc_controller_input origin;

  /** Current input state */
  struct joybus_gc_controller_input input;

  /** Packed input state buffer */
  uint8_t packed_input[8];

  /** Whether the input state is valid */
  bool input_valid;

  /** Callback for controller reset events */
  joybus_gc_controller_reset_cb_t on_reset;

  /** Callback for controller motor state change events */
  joybus_gc_controller_motor_cb_t on_motor_state_change;
};

/**
 * Initialize a GameCube controller.
 *
 * This function sets up the initial state, and registers SI command
 * handlers for OEM GameCube controller, and WaveBird controller commands.
 *
 * @param controller the controller to initialize
 * @param type the device type flags
 */
void joybus_gc_controller_init(struct joybus_gc_controller *controller, uint16_t type);

/**
 * Check if the controller is a WaveBird controller.
 *
 * @param controller the controller to check
 *
 * @return true if the controller is a WaveBird controller
 */
static inline bool joybus_gc_controller_is_wireless(struct joybus_gc_controller *controller)
{
  return joybus_id_get_type(controller->id) & JOYBUS_ID_GCN_WIRELESS;
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
void joybus_gc_controller_set_wireless_id(struct joybus_gc_controller *controller, uint16_t wireless_id);

/**
 * Get the current wireless ID of the controller.
 *
 * @param controller the controller to get the wireless ID from
 *
 * @return the current 10-bit wireless ID
 */
static inline uint16_t joybus_gc_controller_get_wireless_id(struct joybus_gc_controller *controller)
{
  return (controller->id[1] & 0xC0) << 2 | controller->id[2];
}

/**
 * Determine if the wireless ID has been fixed.
 *
 * Fixing the wireless ID is used to bind a WaveBird controller to a specific receiver.
 *
 * @param controller the controller to check
 *
 * @return true if the wireless ID is fixed
 */
static inline bool joybus_gc_controller_wireless_id_fixed(struct joybus_gc_controller *controller)
{
  return joybus_id_get_type(controller->id) & JOYBUS_ID_GCN_WIRELESS_ID_FIXED;
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
static inline void joybus_gc_controller_input_valid(struct joybus_gc_controller *controller, bool valid)
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
void joybus_gc_controller_set_origin(struct joybus_gc_controller *controller,
                                     struct joybus_gc_controller_input *new_origin);

/** @} */