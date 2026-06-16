/**
 * @file
 *
 * Common definitions for N64 controllers.
 */

#pragma once

#include <stdint.h>

/**
 * N64 controller button bitmask flags.
 *
 * These test the `buttons` field of ::joybus_n64_controller_state. Bits 0-7
 * correspond to the first button byte of the wire input state, bits 8-15 to
 * the second.
 */
#define JOYBUS_N64_BUTTON_RIGHT   (1 << 0)
#define JOYBUS_N64_BUTTON_LEFT    (1 << 1)
#define JOYBUS_N64_BUTTON_DOWN    (1 << 2)
#define JOYBUS_N64_BUTTON_UP      (1 << 3)
#define JOYBUS_N64_BUTTON_START   (1 << 4)
#define JOYBUS_N64_BUTTON_Z       (1 << 5)
#define JOYBUS_N64_BUTTON_B       (1 << 6)
#define JOYBUS_N64_BUTTON_A       (1 << 7)
#define JOYBUS_N64_BUTTON_C_RIGHT (1 << 8)
#define JOYBUS_N64_BUTTON_C_LEFT  (1 << 9)
#define JOYBUS_N64_BUTTON_C_DOWN  (1 << 10)
#define JOYBUS_N64_BUTTON_C_UP    (1 << 11)
#define JOYBUS_N64_BUTTON_R       (1 << 12)
#define JOYBUS_N64_BUTTON_L       (1 << 13)
#define JOYBUS_N64_RST            (1 << 15)
#define JOYBUS_N64_BUTTON_MASK    0x3FFF

/**
 * N64 controller input state.
 *
 * Matches the wire format of the input state byte-for-byte on a
 * little-endian CPU (wire byte 0 is the low byte of `buttons`), so it can
 * be sent and received without repacking.
 */
struct joybus_n64_controller_state {
  /// Button state
  uint16_t buttons;

  /// Stick x-axis position, nominally -80..80
  int8_t stick_x;

  /// Stick y-axis position, nominally -80..80
  int8_t stick_y;
} __attribute__((packed));
