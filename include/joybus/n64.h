#pragma once

#include <stdint.h>

/**
 * N64 controller button bitmask flags.
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
 */
struct joybus_n64_controller_input {
  /** Button state */
  uint16_t buttons;

  /** Stick x-axis position */
  uint8_t stick_x;

  /** Stick y-axis position */
  uint8_t stick_y;
} __attribute__((packed));