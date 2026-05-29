/**
 * Checksum routines used by various joybus commands.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Fold one byte into a running CRC-8 (polynomial 0x85).
 *
 * Seed `crc` with 0 on the first call and pass the result back in on subsequent
 * calls to compute the CRC-8 over a stream of bytes.
 *
 * @param crc  Running CRC value, or 0 to start a fresh checksum
 * @param byte Next byte to fold in
 * @return     Updated running CRC after folding in `byte`
 */
uint8_t joybus_crc8_update(uint8_t crc, uint8_t byte);

/**
 * Compute the CRC-8 (polynomial 0x85) over a complete buffer.
 *
 * @param data Buffer to checksum
 * @param size Number of bytes in `data`
 * @return     CRC-8 of the buffer
 */
uint8_t joybus_crc8(const uint8_t *data, size_t size);

/**
 * Compute the 5-bit address checksum for N64 data transfer commands.
 *
 * The address is a 16-bit value whose top 11 bits are the 32-byte-aligned
 * address and whose low 5 bits carry the checksum.
 *
 * @param addr 16-bit address (low 5 bits ignored)
 * @return     5-bit checksum in the low bits of the returned byte
 */
uint8_t joybus_address_checksum(uint16_t addr);

/**
 * Validate the 5-bit address checksum for an N64 data transfer command.
 *
 * @param addr 16-bit address with checksum in the low 5 bits
 * @return     true if the checksum is valid, false if not
 */
bool joybus_address_checksum_valid(uint16_t addr);