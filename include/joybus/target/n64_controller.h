/**
 * @defgroup joybus_target_n64_controller N64 Controller Target
 * @ingroup joybus_target
 *
 * Joybus target implementation for standard N64 controllers.
 *
 * @{
 */

#pragma once

#include <joybus/bus.h>
#include <joybus/identify.h>
#include <joybus/target.h>
#include <joybus/common/n64_controller.h>
#include <joybus/target/n64_pak.h>

struct joybus_target_n64_controller;

/// Macro to cast from a generic Joybus target to an N64 controller target
#define JOYBUS_TARGET_N64_CONTROLLER(target) ((struct joybus_target_n64_controller *)(target))

/**
 * Callback type for N64 controller reset events.
 *
 * @param controller the controller that was reset
 */
typedef void (*joybus_target_n64_controller_reset_cb_t)(struct joybus_target_n64_controller *controller);

/**
 * N64 controller Joybus target.
 */
struct joybus_target_n64_controller {
  /// Base target interface
  struct joybus_target base;

  /// Controller ID
  struct joybus_id id;

  /// Current input state
  struct joybus_n64_controller_state input;

  /// Callback for controller reset events
  joybus_target_n64_controller_reset_cb_t on_reset;

  /// Currently attached pak (if any)
  struct joybus_target_n64_pak *pak;

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
void joybus_target_n64_controller_init(struct joybus_target_n64_controller *controller);

/**
 * Set the reset callback for the controller.
 *
 * @param controller the controller to set the callback for
 * @param callback the callback function
 */
void joybus_target_n64_controller_set_reset_cb(struct joybus_target_n64_controller *controller,
                                               joybus_target_n64_controller_reset_cb_t callback);

/**
 * Attach an pak to the controller.
 *
 * @param controller the controller to attach the pak to
 * @param pak the pak to attach
 */
void joybus_target_n64_controller_attach_pak(struct joybus_target_n64_controller *controller,
                                             struct joybus_target_n64_pak *pak);

/**
 * Detach the currently attached pak from the controller.
 *
 * @param controller the controller to detach the pak from
 */
void joybus_target_n64_controller_detach_pak(struct joybus_target_n64_controller *controller);
/** @} */
