/**
 * @defgroup joybus_target_n64_controller N64 Controller Target
 * Joybus target implementation for standard N64 controllers.
 * @ingroup joybus_target
 * @{
 */

#pragma once

#include <joybus/bus.h>
#include <joybus/n64.h>
#include <joybus/target.h>
#include <joybus/target/n64_accessory.h>

struct joybus_n64_controller;

/// Macro to cast from a generic Joybus target to an N64 controller target
#define JOYBUS_N64_CONTROLLER(target) ((struct joybus_n64_controller *)(target))

/**
 * Callback type for N64 controller reset events.
 *
 * @param controller the controller that was reset
 */
typedef void (*joybus_n64_controller_reset_cb_t)(struct joybus_n64_controller *controller);

/**
 * N64 controller Joybus target.
 */
struct joybus_n64_controller {
  /// Base target interface
  struct joybus_target base;

  /// Controller ID
  uint8_t id[3];

  /// Current input state
  struct joybus_n64_controller_input input;

  /// Callback for controller reset events
  joybus_n64_controller_reset_cb_t on_reset;

  /// Currently attached accessory (if any)
  struct joybus_n64_accessory *accessory;

  /// CRC for data transfer commands
  uint8_t crc;

  /// Response buffer
  uint8_t response[JOYBUS_BLOCK_SIZE];
};

/**
 * Initialize an N64 controller.
 *
 * This function sets up the initial state, and registers Joybus command
 * handlers for OEM N64 controller commands.
 *
 * @param controller the controller to initialize
 */
void joybus_n64_controller_init(struct joybus_n64_controller *controller);

/**
 * Set the reset callback for the controller.
 *
 * NOTE: Reset callbacks are called from interrupt context, do not perform any
 *       blocking operations within the callback.
 *
 * @param controller the controller to set the callback for
 * @param callback the callback function
 */
void joybus_n64_controller_set_reset_callback(struct joybus_n64_controller *controller,
                                              joybus_n64_controller_reset_cb_t callback);

/**
 * Attach an accessory to the controller.
 *
 * @param controller the controller to attach the accessory to
 * @param accessory the accessory to attach
 */
void joybus_n64_controller_attach_accessory(struct joybus_n64_controller *controller,
                                            struct joybus_n64_accessory *accessory);

/**
 * Detach the currently attached accessory from the controller.
 *
 * @param controller the controller to detach the accessory from
 */
void joybus_n64_controller_detach_accessory(struct joybus_n64_controller *controller);
/** @} */