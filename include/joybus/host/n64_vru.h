#pragma once

#include <joybus/bus.h>

/**
 * Read a 36-byte word recognition result from the N64 VRU.
 *
 * @param bus the Joybus to use
 * @param response the buffer to store the response in
 * @return 0 on success, negative error code on failure
 */
int joybus_n64_vru_read36(struct joybus *bus, uint8_t response[36]);

/**
 * Write one 20-byte dictionary-data block to the N64 VRU.
 *
 * @param bus the Joybus to use
 * @param addr the address to write to
 * @param data the data to write
 * @return 0 on success, negative error code on failure
 */
int joybus_n64_vru_write20(struct joybus *bus, uint16_t addr, const uint8_t data[20]);

/**
 * Read the N64 VRU command status register.
 *
 * @param bus the Joybus to use
 * @param addr the address to read from
 * @param response the buffer to store the response in
 * @return 0 on success, negative error code on failure
 */
int joybus_n64_vru_read2(struct joybus *bus, uint16_t addr, uint8_t response[2]);

/**
 * Write a 4-byte control word to the N64 VRU.
 *
 * @param bus the Joybus to use
 * @param addr the address to write to
 * @param data the data to write
 * @return 0 on success, negative error code on failure
 */
int joybus_n64_vru_write4(struct joybus *bus, uint16_t addr, const uint8_t data[4]);

/**
 * Write a configuration byte to the N64 VRU's ADC.
 *
 * @param bus the Joybus to use
 * @param value the value to write
 * @return 0 on success, negative error code on failure
 */
int joybus_n64_vru_swrite(struct joybus *bus, uint8_t value);

/**
 * Helper function to initialize an N64 VRU.
 *
 * @param bus the Joybus to use
 * @return 0 on success, negative error code on failure
 */
int joybus_n64_vru_init(struct joybus *bus);
