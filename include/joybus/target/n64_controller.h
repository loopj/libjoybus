/**
 * @defgroup joybus_target_n64_controller N64 Controller Target
 * N64 controller Joybus target
 * @ingroup joybus_target
 * @{
 */

#pragma once

#include <joybus/n64.h>
#include <joybus/target.h>

struct joybus_n64_controller;

/** Macro to cast to a N64 controller target */
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
  struct joybus_target base;

  /** Controller ID */
  uint8_t id[3];

  /** Current input state */
  struct joybus_n64_controller_input input;

  /** Callback for controller reset events */
  joybus_n64_controller_reset_cb_t on_reset;
};

/**
 * Initialize an N64 controller.
 *
 * This function sets up the initial state, and registers SI command
 * handlers for OEM N64 controller commands.
 *
 * @param controller the controller to initialize
 * @param type the device type flags
 */
void joybus_n64_controller_init(struct joybus_n64_controller *controller, uint8_t type);

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

/** @} */