/**
 * @addtogroup joybus
 *
 * N64 Controller Pak filesystem helpers.
 *
 * A Controller Pak keeps its system area in the first bank: four copies of an
 * ID block in page 0, an inode table and a mirror of it per bank, then two
 * pages of note table. These routines write that area for a given bank count
 * and check whether one is present, so an emulated pak can present itself as
 * formatted. They work on a bank held in memory and do not touch the bus.
 *
 * @{
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/// Size of one bank in bytes, the storage of an original pak
#define JOYBUS_N64_PAK_BANK_SIZE 32768

/// Size of one page in bytes, the unit the filesystem allocates in
#define JOYBUS_N64_PAK_FS_PAGE_SIZE 256

/// The most banks whose system area fits in the first bank
#define JOYBUS_N64_PAK_FS_MAX_BANKS 62

/// Number of system pages in the first bank for a given bank count
#define JOYBUS_N64_PAK_FS_SYSTEM_PAGES(banks) (3 + 2 * (banks))

/**
 * Check whether a bank holds a filesystem for the given bank count.
 *
 * @param bank0 the first bank of the pak, ::JOYBUS_N64_PAK_BANK_SIZE bytes
 * @param banks how many banks the pak presents
 * @return true if an intact ID names that many banks, false otherwise
 */
bool joybus_n64_pak_fs_valid(const uint8_t *bank0, uint8_t banks);

/**
 * Write a fresh filesystem for the given bank count into a bank.
 *
 * Only the system area is written. The data pages keep their contents but are
 * all marked free, so a console sees an empty pak.
 *
 * @param bank0  the first bank of the pak, ::JOYBUS_N64_PAK_BANK_SIZE bytes
 * @param banks  how many banks the pak presents, 1 to ::JOYBUS_N64_PAK_FS_MAX_BANKS
 * @param random value stored in the ID to tell this pak apart from others
 * @return positive number of bytes written from the start of the bank, a negative joybus_error on failure
 */
int joybus_n64_pak_fs_format(uint8_t *bank0, uint8_t banks, uint32_t random);

/** @} */
