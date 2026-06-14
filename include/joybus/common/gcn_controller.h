/**
 * @file
 *
 * Common definitions for GameCube controllers.
 */

#pragma once

#include <stdint.h>

/**
 * GameCube controller button bitmask flags.
 *
 * These test the `buttons` field of ::joybus_gcn_controller_state. Bits 0-7
 * correspond to the first button byte of the wire input state, bits 8-15 to
 * the second.
 */
#define JOYBUS_GCN_BUTTON_A       (1 << 0)
#define JOYBUS_GCN_BUTTON_B       (1 << 1)
#define JOYBUS_GCN_BUTTON_X       (1 << 2)
#define JOYBUS_GCN_BUTTON_Y       (1 << 3)
#define JOYBUS_GCN_BUTTON_START   (1 << 4)
#define JOYBUS_GCN_NEED_ORIGIN    (1 << 5)
#define JOYBUS_GCN_ERROR_LATCH    (1 << 6)
#define JOYBUS_GCN_ERROR          (1 << 7)
#define JOYBUS_GCN_BUTTON_LEFT    (1 << 8)
#define JOYBUS_GCN_BUTTON_RIGHT   (1 << 9)
#define JOYBUS_GCN_BUTTON_DOWN    (1 << 10)
#define JOYBUS_GCN_BUTTON_UP      (1 << 11)
#define JOYBUS_GCN_BUTTON_Z       (1 << 12)
#define JOYBUS_GCN_BUTTON_R       (1 << 13)
#define JOYBUS_GCN_BUTTON_L       (1 << 14)
#define JOYBUS_GCN_USE_ORIGIN     (1 << 15)
#define JOYBUS_GCN_BUTTON_MASK    0x7F1F

/**
 * GameCube controller input/origin state.
 *
 * Matches the wire format of the input state byte-for-byte on a
 * little-endian CPU (wire byte 0 is the low byte of `buttons`), so it can
 * be sent and received without repacking.
 */
struct joybus_gcn_controller_state {
  /// Button state
  uint16_t buttons;

  /// Main stick x-axis position
  uint8_t stick_x;

  /// Main stick y-axis position
  uint8_t stick_y;

  /// C-stick x-axis position
  uint8_t substick_x;

  /// C-stick y-axis position
  uint8_t substick_y;

  /// Left analog trigger position
  uint8_t trigger_left;

  /// Right analog trigger position
  uint8_t trigger_right;

  /// Analog A button value
  uint8_t analog_a;

  /// Analog B button value
  uint8_t analog_b;
} __attribute__((packed));

/**
 * Analog modes for packing GameCube controller input state.
 *
 * The "read" command used by games expects 8-byte responses, this is presumably
 * so it fits in a nice round multiple of 32-bit words. The full input state is
 * 10 bytes long, so there are various ways to "pack" the input state into 8
 * bytes. Depending on the analog mode, either one pair of analog inputs can be
 * omitted, or two pairs of analog inputs can be truncated to 4 bits.
 *
 * All production games, with the exception of Luigi's Mansion, use analog mode 3.
 * This mode omits the analog A/B inputs, and sends the substick X/Y and triggers
 * at full precision. Analog A/B buttons were only present in pre-production
 * GameCube controllers.
 */
enum joybus_gcn_analog_mode {
  /// Substick X/Y full precision, triggers and analog A/B truncated to 4 bits
  JOYBUS_GCN_ANALOG_MODE_0,

  /// Triggers full precision, substick X/Y and analog A/B truncated to 4 bits
  JOYBUS_GCN_ANALOG_MODE_1,

  /// Analog A/B full precision, substick X/Y and triggers truncated to 4 bits
  JOYBUS_GCN_ANALOG_MODE_2,

  /// Substick X/Y and triggers full precision, analog A/B omitted
  JOYBUS_GCN_ANALOG_MODE_3,

  /// Substick X/Y and analog A/B full precision, triggers omitted
  JOYBUS_GCN_ANALOG_MODE_4,
};

/**
 * GameCube controller motor states.
 */
enum joybus_gcn_motor_state {
  /// Stop the rumble motor
  JOYBUS_GCN_MOTOR_STOP,

  /// Start the rumble motor
  JOYBUS_GCN_MOTOR_RUMBLE,

  /// Stop the rumble motor immediately
  JOYBUS_GCN_MOTOR_STOP_HARD,
};
