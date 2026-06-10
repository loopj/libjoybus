/**
 * @addtogroup joybus_host
 *
 * @{
 */

#pragma once

#include <joybus/bus.h>
#include <joybus/common/n64_controller.h>

/**
 * N64 controller accessory types.
 */
enum joybus_n64_accessory_type {
  /// No accessory connected
  JOYBUS_N64_ACCESSORY_NONE = 0,

  /// Accessory type could not be determined
  JOYBUS_N64_ACCESSORY_UNKNOWN,

  /// Controller Pak
  JOYBUS_N64_ACCESSORY_CONTROLLER_PAK,

  /// Rumble Pak
  JOYBUS_N64_ACCESSORY_RUMBLE_PAK,

  /// Transfer Pak
  JOYBUS_N64_ACCESSORY_TRANSFER_PAK,

  /// Bio Sensor
  JOYBUS_N64_ACCESSORY_BIO_SENSOR,

  /// Snap Station
  JOYBUS_N64_ACCESSORY_SNAP_STATION,
};

/**
 * Callback type for N64 accessory detection.
 *
 * @param accessory_type the detected accessory type, one of JOYBUS_N64_ACCESSORY_*
 * @param user_data user data passed to the detection function
 */
typedef void (*joybus_n64_accessory_detect_cb_t)(int accessory_type, void *user_data);

/**
 * Read the current input state of an N64 controller.
 *
 * @param bus the Joybus to use
 * @param response buffer to store the response in, must be at least JOYBUS_CMD_N64_READ_RX bytes
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_n64_read(struct joybus *bus, uint8_t *response, joybus_transfer_cb_t callback, void *user_data);

/**
 * Read data from a N64 controller's accessory port.
 *
 * Address checksum is automatically calculated.
 *
 * @param bus the Joybus to use
 * @param addr the address to read from, must be 32-byte aligned
 * @param response buffer to store the response in, must be at least JOYBUS_CMD_N64_ACCESSORY_READ_RX bytes
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_n64_accessory_read(struct joybus *bus, uint16_t addr, uint8_t *response, joybus_transfer_cb_t callback,
                              void *user_data);

/**
 * Write data to a N64 controller's accessory port.
 *
 * Address checksum is automatically calculated.
 *
 * @param bus the Joybus to use
 * @param addr the address to write to, must be 32-byte aligned
 * @param data the 32-byte data to write
 * @param response buffer to store the response in, must be at least JOYBUS_CMD_N64_WRITE_MEM_RX bytes
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_n64_accessory_write(struct joybus *bus, uint16_t addr, const uint8_t *data, uint8_t *response,
                               joybus_transfer_cb_t callback, void *user_data);

/**
 * Read an 8-byte block from a N64 cartridge EEPROM.
 *
 * @param bus the Joybus to use
 * @param addr the block address to read from, (0-63 for 4k EEPROMs / 0-255 for 16k EEPROMs)
 * @param response buffer to store the response in, must be at least JOYBUS_CMD_N64_EEPROM_READ_RX bytes
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_n64_eeprom_read(struct joybus *bus, uint8_t addr, uint8_t *response, joybus_transfer_cb_t callback,
                           void *user_data);

/**
 * Write an 8-byte block to a N64 cartridge EEPROM.
 *
 * @param bus the Joybus to use
 * @param addr the block address to write to, (0-63 for 4k EEPROMs / 0-255 for 16k EEPROMs)
 * @param data the 8-byte data to write
 * @param callback a callback function to call when the transfer is complete
 * @param user_data user data to pass to the callback function
 * @return 0 on success, negative error code on failure
 */
int joybus_n64_eeprom_write(struct joybus *bus, uint8_t addr, const uint8_t *data, joybus_transfer_cb_t callback,
                            void *user_data);

/**
 * Helper function to detect the accessory connected to a N64 controller.
 *
 * This function initiates an asynchronous sequence of commands to detect the accessory type.
 * The provided callback will be called with the detected accessory type once the detection
 * sequence is complete.
 *
 * @param bus the Joybus to use
 * @param callback a callback function to call when the detection is complete
 * @param user_data user data to pass to the callback function
 */
void joybus_n64_accessory_detect(struct joybus *bus, joybus_n64_accessory_detect_cb_t callback, void *user_data);

/**
 * Helper function to start the rumble motor in a N64 Rumble Pak.
 *
 * @param bus the Joybus to use
 */
void joybus_n64_motor_start(struct joybus *bus);

/**
 * Helper function to stop the rumble motor in a N64 Rumble Pak.
 *
 * @param bus the Joybus to use
 */
void joybus_n64_motor_stop(struct joybus *bus);

/** @} */
