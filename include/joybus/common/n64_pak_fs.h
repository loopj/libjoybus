/**
 * @addtogroup joybus
 *
 * N64 Controller Pak filesystem helpers.
 *
 * A Controller Pak keeps its system area in the first bank: four copies of an
 * ID block in page 0, an inode table and a mirror of it per bank, then two
 * pages of note table. These routines write that area for a given bank count
 * and check whether one is present, so an emulated pak can present itself as
 * formatted. They work on a bank held in memory, or a block or page at a time
 * for a pak whose banks are not, and do not touch the bus.
 *
 * @{
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <joybus/common/n64_pak.h>

/// Size of one page in bytes, the unit the filesystem allocates in
#define JOYBUS_N64_PAK_FS_PAGE_SIZE 256

/// The most banks whose system area fits in the first bank
#define JOYBUS_N64_PAK_FS_MAX_BANKS 62

/// Page of the first bank holding the four ID copies
#define JOYBUS_N64_PAK_FS_ID_PAGE 0

/// Page of the first bank holding a bank's inode table
#define JOYBUS_N64_PAK_FS_INODE_PAGE(bank) (1 + (bank))

/// Page of the first bank holding the mirror of a bank's inode table, for a given bank count
#define JOYBUS_N64_PAK_FS_INODE_MIRROR_PAGE(banks, bank) (1 + (banks) + (bank))

/// First page of the note table in the first bank, for a given bank count
#define JOYBUS_N64_PAK_FS_NOTE_TABLE_PAGE(banks) (1 + 2 * (banks))

/// Number of pages the note table takes
#define JOYBUS_N64_PAK_FS_NOTE_TABLE_PAGES 2

/// Number of system pages in the first bank for a given bank count
#define JOYBUS_N64_PAK_FS_SYSTEM_PAGES(banks) (JOYBUS_N64_PAK_FS_NOTE_TABLE_PAGE(banks) + JOYBUS_N64_PAK_FS_NOTE_TABLE_PAGES)

/// First page of a bank the filesystem allocates, for a given bank count. Page 0 of every later bank is reserved
#define JOYBUS_N64_PAK_FS_FIRST_DATA_PAGE(banks, bank) ((bank) == 0 ? JOYBUS_N64_PAK_FS_SYSTEM_PAGES(banks) : 1)

/**
 * Check whether a block address in the first bank holds a copy of the ID.
 *
 * @param addr block-aligned address within the first bank
 * @return true if the block is one of the four ID copies, false otherwise
 */
bool joybus_n64_pak_fs_is_id_block(uint16_t addr);

/**
 * Read the bank count an ID block names.
 *
 * @param block an ID block, ::JOYBUS_N64_PAK_BLOCK_SIZE bytes
 * @return the bank count in the block
 */
uint8_t joybus_n64_pak_fs_id_banks(const uint8_t block[JOYBUS_N64_PAK_BLOCK_SIZE]);

/**
 * Check whether an ID block is intact and names the given bank count.
 *
 * A pak holds a filesystem when any one of its four ID copies passes.
 *
 * @param block an ID block, ::JOYBUS_N64_PAK_BLOCK_SIZE bytes
 * @param banks how many banks the pak presents
 * @return true if both checksums match, the block marks a Controller Pak, and it names that many banks
 */
bool joybus_n64_pak_fs_id_valid(const uint8_t block[JOYBUS_N64_PAK_BLOCK_SIZE], uint8_t banks);

/**
 * Check whether a bank holds a filesystem for the given bank count.
 *
 * Only the ID copies in page 0 are read.
 *
 * @param bank0 the first bank of the pak, at least ::JOYBUS_N64_PAK_FS_PAGE_SIZE bytes
 * @param banks how many banks the pak presents
 * @return true if an intact ID names that many banks, false otherwise
 */
bool joybus_n64_pak_fs_valid(const uint8_t *bank0, uint8_t banks);

/**
 * Fill one page of a fresh system area for the given bank count.
 *
 * Every page from 0 to JOYBUS_N64_PAK_FS_SYSTEM_PAGES(banks) - 1, each written
 * at its index in the first bank, together format the pak.
 *
 * @param page   the page to fill, ::JOYBUS_N64_PAK_FS_PAGE_SIZE bytes
 * @param index  the page's index in the first bank, below JOYBUS_N64_PAK_FS_SYSTEM_PAGES(banks)
 * @param banks  how many banks the pak presents, 1 to ::JOYBUS_N64_PAK_FS_MAX_BANKS
 * @param random value stored in the ID to tell this pak apart from others
 * @return 0 on success, -JOYBUS_ERR_INVALID_ARG for a bank count or index out of range
 */
int joybus_n64_pak_fs_format_page(uint8_t page[JOYBUS_N64_PAK_FS_PAGE_SIZE], uint8_t index, uint8_t banks,
                                  uint32_t random);

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
