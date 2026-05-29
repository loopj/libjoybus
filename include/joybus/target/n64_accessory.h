/**
 * @defgroup joybus_target_n64_accessory N64 Controller Accessory
 * Base interface for N64 controller accessories (rumble pak, controller pak, etc.)
 * @ingroup joybus_target_n64_controller
 * @{
 */

#pragma once

#include <stdint.h>

#include <joybus/commands.h>

struct joybus_n64_accessory;

/// Macro to cast a concrete N64 accessory instance to a generic N64 accessory instance.
#define JOYBUS_N64_ACCESSORY(acc) ((struct joybus_n64_accessory *)(acc))

/**
 * API for implementing an N64 controller accessory.
 */
struct joybus_n64_accessory_api {
  /**
   * Read a 32-byte block from the accessory.
   *
   * Called from interrupt context, on the response critical path.
   *
   * @param accessory the accessory being read from
   * @param addr      block-aligned address (low 5 bits are zero)
   * @param buf       destination buffer, exactly 32 bytes
   */
  void (*read_block)(struct joybus_n64_accessory *accessory, uint16_t addr, uint8_t buf[JOYBUS_ACCESSORY_BLOCK_SIZE]);

  /**
   * Write a 32-byte block to the accessory.
   *
   * Called from interrupt context, AFTER the CRC response has been sent.
   *
   * @param accessory the accessory being written to
   * @param addr      block-aligned address (low 5 bits are zero)
   * @param buf       source buffer, exactly 32 bytes
   */
  void (*write_block)(struct joybus_n64_accessory *accessory, uint16_t addr,
                      const uint8_t buf[JOYBUS_ACCESSORY_BLOCK_SIZE]);
};

/**
 * An N64 controller accessory, such as a rumble pak or controller pak.
 */
struct joybus_n64_accessory {
  /// API for handling reads and writes to the accessory
  const struct joybus_n64_accessory_api *api;
};

/** @} */
