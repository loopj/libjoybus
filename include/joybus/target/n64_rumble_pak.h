/**
 * @defgroup joybus_target_n64_rumble_pak N64 Rumble Pak
 * @ingroup joybus_target_n64_pak
 *
 * N64 pak implementation which emulates a Rumble Pak, providing a callback for motor state changes.
 *
 * @{
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <joybus/target/n64_pak.h>

struct joybus_target_n64_rumble_pak;

/// Macro to cast from a generic N64 pak to a rumble pak
#define JOYBUS_TARGET_N64_RUMBLE_PAK(pak) ((struct joybus_target_n64_rumble_pak *)(pak))

/**
 * Callback type for rumble pak motor state change events.
 *
 * @param pak    the rumble pak whose motor state changed
 * @param active true if the motor should be on, false if off
 */
typedef void (*joybus_target_n64_rumble_pak_motor_cb)(struct joybus_target_n64_rumble_pak *pak, bool active);

/**
 * N64 Rumble Pak pak.
 */
struct joybus_target_n64_rumble_pak {
  /// Base pak interface
  struct joybus_target_n64_pak base;

  /// Rumble pak enabled
  bool enabled;

  /// Current motor state
  bool active;

  /// Callback for motor state change events
  joybus_target_n64_rumble_pak_motor_cb on_motor_change;
};

/**
 * Initialize a rumble pak.
 *
 * @param pak the rumble pak to initialize
 */
void joybus_target_n64_rumble_pak_init(struct joybus_target_n64_rumble_pak *pak);

/**
 * Set the motor state change callback for the rumble pak.
 *
 * NOTE: Motor callbacks are called from interrupt context, do not perform
 *       any blocking operations within the callback.
 *
 * @param pak      the rumble pak to set the callback for
 * @param callback the callback function
 */
void joybus_target_n64_rumble_pak_set_motor_cb(struct joybus_target_n64_rumble_pak *pak,
                                               joybus_target_n64_rumble_pak_motor_cb callback);
/** @} */
