/**
 * @defgroup joybus_target_n64_pak_controller N64 Controller Pak
 * @ingroup joybus_target_n64_pak
 *
 * N64 pak implementation which emulates a Controller Pak, the battery backed
 * save memory that plugs into a controller. The pak behaviour lives here, and
 * the storage behind it is a pair of block callbacks, or a buffer for the
 * simple case.
 *
 * An original pak is one 32 KB bank, which a console formats and mounts on
 * its own. Larger third party paks hold several banks and present one at a
 * time, switched by a write to the probe area at 0x8000. A banked pak from
 * this target must be formatted with joybus_n64_pak_fs_format() before use.
 * A select past the last bank is ignored, which keeps an accessory probe from
 * moving the bank but also stops a console counting the banks of a blank pak.
 *
 * A write to an ID block that names another bank count is refused with a
 * transfer error, so the pak cannot be reformatted to a different shape.
 *
 * @{
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <joybus/bus.h>
#include <joybus/target/n64_pak.h>

struct joybus_target_n64_pak_controller;

/// Macro to cast from a generic N64 pak to a controller pak
#define JOYBUS_TARGET_N64_PAK_CONTROLLER(pak) ((struct joybus_target_n64_pak_controller *)(pak))

/**
 * Callback type for reading a block of a bank.
 *
 * Runs in interrupt context, on the response critical path, so it must return
 * quickly. Mark the implementation with ::JOYBUS_RAM_FUNC.
 *
 * @param pak  the controller pak being read from
 * @param bank the bank the console has selected
 * @param addr block-aligned address within the bank, below 0x8000
 * @param buf  destination buffer, exactly 32 bytes
 * @return 0 on success, -JOYBUS_ERR_BUSY to have the console retry, another negative joybus_error on failure
 */
typedef int (*joybus_target_n64_pak_controller_read_cb)(struct joybus_target_n64_pak_controller *pak, uint8_t bank,
                                                        uint16_t addr, uint8_t buf[JOYBUS_N64_PAK_BLOCK_SIZE]);

/**
 * Callback type for writing a block of a bank.
 *
 * The console retries a declined write with the same block, so storage must
 * not act on one it declines. Runs in interrupt context, on the response
 * critical path, so it must return quickly. Mark the implementation with
 * ::JOYBUS_RAM_FUNC.
 *
 * @param pak  the controller pak being written to
 * @param bank the bank the console has selected
 * @param addr block-aligned address within the bank, below 0x8000
 * @param buf  source buffer, exactly 32 bytes
 * @return 0 on success, -JOYBUS_ERR_BUSY to have the console retry, another negative joybus_error on failure
 */
typedef int (*joybus_target_n64_pak_controller_write_cb)(struct joybus_target_n64_pak_controller *pak, uint8_t bank,
                                                         uint16_t addr, const uint8_t buf[JOYBUS_N64_PAK_BLOCK_SIZE]);

/**
 * Callback type for bank select events.
 *
 * Runs in interrupt context, so it must return quickly.
 *
 * @param pak  the controller pak
 * @param bank the bank now selected
 */
typedef void (*joybus_target_n64_pak_controller_select_cb)(struct joybus_target_n64_pak_controller *pak, uint8_t bank);

/**
 * Callback type for block written events, fired after storage has taken a write.
 *
 * Runs in interrupt context, so it must return quickly.
 *
 * @param pak  the controller pak
 * @param bank the bank written
 * @param addr block-aligned address within the bank
 */
typedef void (*joybus_target_n64_pak_controller_written_cb)(struct joybus_target_n64_pak_controller *pak, uint8_t bank,
                                                            uint16_t addr);

/**
 * N64 Controller Pak pak.
 */
struct joybus_target_n64_pak_controller {
  /// Base pak interface
  struct joybus_target_n64_pak base;

  /// Callback that reads a block from storage, the pak answers busy until set
  joybus_target_n64_pak_controller_read_cb read;

  /// Callback that writes a block to storage, the pak answers busy until set
  joybus_target_n64_pak_controller_write_cb write;

  /// Pointer handed to the storage callbacks
  void *user_data;

  /// Callback for bank select events
  joybus_target_n64_pak_controller_select_cb on_select;

  /// Callback for block written events
  joybus_target_n64_pak_controller_written_cb on_written;

  /// Banks the pak presents, 1 for an original pak
  uint8_t banks;

  /// Bank the console has selected, always 0 on a one bank pak
  uint8_t selected;
};

/**
 * Initialize a controller pak.
 *
 * The pak answers busy until storage is set with
 * joybus_target_n64_pak_controller_set_storage() or
 * joybus_target_n64_pak_controller_set_memory().
 *
 * @param pak   the controller pak to initialize
 * @param banks how many banks it presents, 1 to ::JOYBUS_N64_PAK_FS_MAX_BANKS, not checked here
 */
void joybus_target_n64_pak_controller_init(struct joybus_target_n64_pak_controller *pak, uint8_t banks);

/**
 * Set the storage behind the pak.
 *
 * For a pak that cannot be held in memory, such as one of many banks kept in
 * flash. The callbacks may answer busy while a bank is fetched, which the
 * console retries.
 *
 * @param pak       the controller pak
 * @param read      callback that reads a block of a bank
 * @param write     callback that writes a block of a bank
 * @param user_data pointer handed back to the callbacks through `pak->user_data`
 */
void joybus_target_n64_pak_controller_set_storage(struct joybus_target_n64_pak_controller *pak,
                                                  joybus_target_n64_pak_controller_read_cb read,
                                                  joybus_target_n64_pak_controller_write_cb write, void *user_data);

/**
 * Back the pak with memory, `banks` x ::JOYBUS_N64_PAK_BANK_SIZE bytes owned by the caller.
 *
 * The buffer is read and written in place, so persisting it is up to the
 * caller, which the written callback helps with. A fresh buffer needs a
 * filesystem before a console will mount it, see joybus_n64_pak_fs_format().
 * A console can format a one bank pak itself, a banked pak it cannot.
 *
 * @param pak    the controller pak
 * @param memory the banks, back to back
 */
void joybus_target_n64_pak_controller_set_memory(struct joybus_target_n64_pak_controller *pak, uint8_t *memory);

/**
 * Set the bank select callback for the controller pak.
 *
 * Storage that keeps only some banks close at hand can start fetching the
 * new one here rather than at its first read.
 *
 * NOTE: Select callbacks are called from interrupt context, do not perform
 *       any blocking operations within the callback.
 *
 * @param pak      the controller pak to set the callback for
 * @param callback the callback function
 */
void joybus_target_n64_pak_controller_set_select_cb(struct joybus_target_n64_pak_controller *pak,
                                                    joybus_target_n64_pak_controller_select_cb callback);

/**
 * Set the block written callback for the controller pak.
 *
 * NOTE: Written callbacks are called from interrupt context, do not perform
 *       any blocking operations within the callback.
 *
 * @param pak      the controller pak to set the callback for
 * @param callback the callback function
 */
void joybus_target_n64_pak_controller_set_written_cb(struct joybus_target_n64_pak_controller *pak,
                                                     joybus_target_n64_pak_controller_written_cb callback);

/** @} */
