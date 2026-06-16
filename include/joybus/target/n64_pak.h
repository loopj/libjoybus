/**
 * @defgroup joybus_target_n64_pak N64 Pak
 * @ingroup joybus_target_n64_controller
 *
 * Interface for implementing N64 "Pak" behavior (Rumble Pak, Controller
 * Pak, etc.) when attached to an N64 controller target.
 *
 * A pak plugs into the expansion port (officially the "Joyport") on the back
 * of an N64 controller, such as a Rumble Pak, Controller Pak, or Transfer
 * Pak. The console reaches it indirectly: it sends read and write commands
 * to the controller, which forwards them to whatever pak is attached.
 *
 * Every pak presents a flat 16-bit address space, read and written 32 bytes
 * at a time (::JOYBUS_PAK_BLOCK_SIZE). It is up to the pak how it manages
 * the address space, and what reads and writes to each address do. For
 * example, a Controller Pak uses the address space to map to 32 KB of
 * storage, but a Rumble Pak treats the addresses as control registers for
 * the rumble motor.
 *
 * The protocol details (command framing, the address checksum, and the data
 * CRC) are handled by libjoybus, so pak implementations just need to
 * implement the logic for responding to reads and writes at each address.
 *
 * To create your own pak, define a struct whose first member is a
 * ::joybus_target_n64_pak (so it can be cast through ::JOYBUS_TARGET_N64_PAK),
 * point its api at a ::joybus_target_n64_pak_api table, and attach it with
 * joybus_target_n64_controller_attach_pak().
 *
 * @{
 */

#pragma once

#include <stdint.h>

#include <joybus/bus.h>

struct joybus_target_n64_pak;

/// Macro to cast a concrete N64 pak instance to a generic N64 pak instance.
#define JOYBUS_TARGET_N64_PAK(pak) ((struct joybus_target_n64_pak *)(pak))

/**
 * API for implementing an N64 pak.
 */
struct joybus_target_n64_pak_api {
  /**
   * Called when a host requests to read a 32-byte block from the pak.
   *
   * Runs in interrupt context, on the response critical path, so it must
   * return quickly.
   *
   * @param pak  the pak being read from
   * @param addr block-aligned address (low 5 bits are zero)
   * @param buf  destination buffer, exactly 32 bytes
   */
  void (*read_block)(struct joybus_target_n64_pak *pak, uint16_t addr, uint8_t buf[JOYBUS_PAK_BLOCK_SIZE]);

  /**
   * Called when a host requests to write a 32-byte block to the pak.
   *
   * Runs in interrupt context, AFTER the CRC response has been sent, but
   * still on the command handling path, so must return before the next command
   * byte is received.
   *
   * @param pak  the pak being written to
   * @param addr block-aligned address (low 5 bits are zero)
   * @param buf  source buffer, exactly 32 bytes
   */
  void (*write_block)(struct joybus_target_n64_pak *pak, uint16_t addr, const uint8_t buf[JOYBUS_PAK_BLOCK_SIZE]);
};

/**
 * An N64 pak, such as a Rumble Pak or Controller Pak.
 */
struct joybus_target_n64_pak {
  /// API for handling reads and writes to the pak
  const struct joybus_target_n64_pak_api *api;
};

/** @} */
