/**
 * CRC-8 with polynomial 0x85, used by various joybus commands that transfer
 * blocks of data (e.g. N64 controller pak read/write).
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Fold one byte into a running CRC-8. Seed `crc` with 0 on the first call
 * and pass the result back in on subsequent calls to compute the CRC-8 over a
 * stream of bytes.
 *
 * @param crc  Running CRC value, or 0 to start a fresh checksum
 * @param byte Next byte to fold in
 * @return     Updated running CRC after folding in `byte`
 */
uint8_t joybus_crc8_update(uint8_t crc, uint8_t byte);

/**
 * Compute the CRC-8 over a complete buffer.
 *
 * @param data Buffer to checksum
 * @param size Number of bytes in `data`
 * @return     CRC-8 of the buffer
 */
uint8_t joybus_crc8(const uint8_t *data, size_t size);
