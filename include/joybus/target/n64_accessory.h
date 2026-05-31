/**
 * @defgroup joybus_target_n64_accessory N64 Controller Accessory
 * @ingroup joybus_target_n64_controller
 *
 * Interface for implementing N64 controller accessories (rumble pak,
 * controller pak, etc.)
 *
 * An N64 controller accessory plugs into the expansion port (officially the
 * "Joyport") on the back of an N64 controller, such as a Rumble Pak,
 * Controller Pak, or Transfer Pak. The console reaches it indirectly: it
 * sends read and write commands to the controller, which forwards them to
 * whatever accessory is attached.
 *
 * Every accessory presents a flat 16-bit address space, read and written 32
 * bytes at a time (::JOYBUS_ACCESSORY_BLOCK_SIZE). It is up to the accessory
 * how it manages the address space, and what reads and writes to each address
 * do. For example, a Controller Pak uses the address space to map to 32 KB of
 * storage, but a Rumble Pak treats the addresses as control registers for the
 * rumble motor.
 *
 * The protocol details (command framing, the address checksum, and the data
 * CRC) are handled by libjoybus, so accessory implementations just need to
 * implement the logic for responding to reads and writes at each address.
 *
 * To create your own accessory, define a struct whose first member is a
 * ::joybus_n64_accessory (so it can be cast through ::JOYBUS_N64_ACCESSORY),
 * point its api at a ::joybus_n64_accessory_api table implementing read_block
 * and write_block, and attach it with joybus_n64_controller_attach_accessory().
 *
 * ### Example
 *
 * ```c
 * // Implement the read/write handlers for your accessory
 * static void my_accessory_read(struct joybus_n64_accessory *acc, uint16_t addr, uint8_t buf[32]) {
 *   // Fill buf with the 32 bytes the console should receive for this block
 * }
 *
 * static void my_accessory_write(struct joybus_n64_accessory *acc, uint16_t addr, const uint8_t buf[32]) {
 *   // Handle the 32 bytes the console sent for this block
 * }
 *
 * // Expose the handlers through an API table
 * static const struct joybus_n64_accessory_api my_accessory_api = {
 *   .read_block = my_accessory_read,
 *   .write_block = my_accessory_write,
 * };
 *
 * // Define the accessory struct
 * struct my_accessory {
 *   // Base accessory interface, must be first member for casting to work
 *   struct joybus_n64_accessory base;
 *
 *   // Any custom state your accessory needs goes here
 * };
 *
 * // Provide an init function to set up the api pointer and any state
 * void my_accessory_init(struct my_accessory *acc) {
 *   // Attach the API table
 *   acc->base.api = &my_accessory_api;
 *
 *   // Initialize any custom state here
 * }
 * ```
 *
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
   * Called when a host requests to read a 32-byte block from the accessory.
   *
   * Runs in interrupt context, on the response critical path, so it must
   * return quickly.
   *
   * @param accessory the accessory being read from
   * @param addr      block-aligned address (low 5 bits are zero)
   * @param buf       destination buffer, exactly 32 bytes
   */
  void (*read_block)(struct joybus_n64_accessory *accessory, uint16_t addr, uint8_t buf[JOYBUS_ACCESSORY_BLOCK_SIZE]);

  /**
   * Called when a host requests to write a 32-byte block to the accessory.
   *
   * Runs in interrupt context, AFTER the CRC response has been sent, but
   * still on the command handling path, so must return before the next command
   * byte is received.
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
