/**
 * @addtogroup joybus
 *
 * Checksum routines used by various Joybus data transfer commands.
 *
 * @{
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Fold one byte into a running Joybus data checksum.
 *
 * Seed `crc` with 0 on the first call and pass the result back in on subsequent
 * calls to compute the checksum over a stream of bytes.
 *
 * @param crc running checksum value, or 0 to start a fresh checksum
 * @param byte next byte to fold in
 * @return updated running checksum after folding in `byte`
 */
uint8_t joybus_data_checksum_update(uint8_t crc, uint8_t byte);

/**
 * Compute the CRC-8 checksum of a Joybus data buffer.
 *
 * @param data buffer to checksum
 * @param size number of bytes in `data`
 * @return the CRC-8 checksum of the buffer
 */
uint8_t joybus_data_checksum(const uint8_t *data, size_t size);

/**
 * Compute the CRC-5 address checksum for data transfer commands.
 *
 * @param addr an 11 bit address to checksum
 * @return CRC-5 checksum of the address
 */
uint8_t joybus_address_checksum(uint16_t addr);

/** @} */
