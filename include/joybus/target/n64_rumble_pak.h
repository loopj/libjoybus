/**
 * @defgroup joybus_target_n64_rumble_pak N64 Rumble Pak
 * N64 rumble pak accessory which exposes a callback for motor state changes.
 * @ingroup joybus_target_n64_accessory
 * @{
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <joybus/target/n64_accessory.h>

struct joybus_n64_rumble_pak;

/// Macro to cast from a generic N64 accessory to a rumble pak
#define JOYBUS_N64_RUMBLE_PAK(pak) ((struct joybus_n64_rumble_pak *)(pak))

/**
 * Callback type for rumble pak motor state change events.
 *
 * @param pak    the rumble pak whose motor state changed
 * @param active true if the motor should be on, false if off
 */
typedef void (*joybus_n64_rumble_pak_motor_cb_t)(struct joybus_n64_rumble_pak *pak, bool active);

/**
 * N64 Rumble Pak accessory.
 */
struct joybus_n64_rumble_pak {
  /// Base accessory interface
  struct joybus_n64_accessory base;

  /// Current motor state
  bool active;

  /// Callback for motor state change events
  joybus_n64_rumble_pak_motor_cb_t on_motor_change;
};

/**
 * Initialize a rumble pak.
 *
 * @param pak the rumble pak to initialize
 */
void joybus_n64_rumble_pak_init(struct joybus_n64_rumble_pak *pak);

/**
 * Set the motor state change callback for the rumble pak.
 *
 * NOTE: Motor callbacks are called from interrupt context, do not perform
 *       any blocking operations within the callback.
 *
 * @param pak      the rumble pak to set the callback for
 * @param callback the callback function
 */
void joybus_n64_rumble_pak_set_motor_callback(struct joybus_n64_rumble_pak *pak,
                                              joybus_n64_rumble_pak_motor_cb_t callback);
/** @} */
