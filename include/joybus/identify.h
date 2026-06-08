/**
 * @file identify.h
 *
 * @brief Joybus device identification values and utilities
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * Joybus device type flags.
 *
 * The first two bytes of the identify buffer contain "type" flags. The
 * joybus_id_get_type function combines them in big-endian/wire order so we
 * can use them as a single 16-bit bitfield.
 */

// Non-GameCube device types
#define JOYBUS_TYPE_N64_MASK                  0x071f  ///< N64 device type mask
#define JOYBUS_TYPE_N64_VRU                   0x0001  ///< N64 Voice Recognition Unit (NUS-020)
#define JOYBUS_TYPE_N64_KEYBOARD              0x0002  ///< N64 Randnet Keyboard (RND-001)
#define JOYBUS_TYPE_GBA_CABLE                 0x0004  ///< GBA connected via GameCube Game Boy Advance cable (DOL-011)
#define JOYBUS_TYPE_N64_RTC                   0x0010  ///< N64 cartridge RTC is present
#define JOYBUS_TYPE_N64_EEPROM_16K            0x0040  ///< N64 cartridge EEPROM is 16KB (otherwise 4KB)
#define JOYBUS_TYPE_N64_EEPROM                0x0080  ///< N64 cartridge EEPROM is present
#define JOYBUS_TYPE_N64_ABSOLUTE              0x0100  ///< N64 device reports absolute position
#define JOYBUS_TYPE_N64_RELATIVE              0x0200  ///< N64 device reports relative position
#define JOYBUS_TYPE_N64_JOYPORT               0x0400  ///< N64 device has a Joyport (accessory port)

// GameCube device types
#define JOYBUS_TYPE_GCN_MASK                  0x1800  ///< GameCube device type mask
#define JOYBUS_TYPE_GCN_WIRELESS_ID_FIXED     0x0010  ///< Wireless ID has been fixed
#define JOYBUS_TYPE_GCN_KEYBOARD              0x0020  ///< GameCube ASCII keyboard
#define JOYBUS_TYPE_GCN_WIRELESS_ORIGIN       0x0020  ///< Wireless origin available
#define JOYBUS_TYPE_GCN_WIRELESS_ID_MASK      0x00C0  ///< Top 2 bits of wireless ID
#define JOYBUS_TYPE_GCN_STANDARD              0x0100  ///< Standard GameCube controller
#define JOYBUS_TYPE_GCN_WIRELESS_STATE        0x0200  ///< Wireless state available
#define JOYBUS_TYPE_GCN_DEVICE                0x0800  ///< Device is a GameCube device
#define JOYBUS_TYPE_GCN_NO_MOTOR              0x2000  ///< No rumble motor present
#define JOYBUS_TYPE_GCN_WIRELESS_RECEIVED     0x4000  ///< Wireless receiver has received a packet
#define JOYBUS_TYPE_GCN_WIRELESS              0x8000  ///< Controller is wireless

/*
 * Joybus device status flags.
 *
 * The last byte of the identify response contains status flags for the
 * controller.
 */

// Status flags for N64 controllers with a Joyport
#define JOYBUS_STATUS_N64_ACCESSORY_PRESENT   0x01    ///< Accessory present
#define JOYBUS_STATUS_N64_ACCESSORY_PULLED    0x02    ///< Accessory removal/change detected
#define JOYBUS_STATUS_N64_ADDR_CHECKSUM_ERROR 0x04    ///< Address checksum error

// Status flags for N64 Voice Recognition Unit
#define JOYBUS_STATUS_N64_VRU_INITIALIZED     0x01    ///< VRU is initialized

// Status flags for non-wireless GameCube controllers
#define JOYBUS_STATUS_GCN_ANALOG_MODE_MASK    0x07    ///< Bits 2–0: Last analog mode
#define JOYBUS_STATUS_GCN_MOTOR_STATE_MASK    0x18    ///< Bits 4–3: Last motor state
#define JOYBUS_STATUS_GCN_NEED_ORIGIN         0x20    ///< New origin data available (host should read origin)
#define JOYBUS_STATUS_GCN_ERROR_LATCHED       0x40    ///< Latched error
#define JOYBUS_STATUS_GCN_ERROR               0x80    ///< Last error
#define JOYBUS_STATUS_GCN_MOTOR_STATE_SHIFT   3
#define JOYBUS_STATUS_GCN_ANALOG_MODE_SHIFT   0

// Status flags for wireless GameCube controllers
#define JOYBUS_STATUS_GCN_WIRELESS_ID_MASK    0xFF    ///< Lower 8 bits of wireless ID

/*
 * Complete type flags a device reports at power on.
 *
 * Most controllers keep the type field constant, but since wireless GameCube
 * controllers use the status field to hold the wireless ID, they add flags
 * to the type field at runtime instead to indicate their state.
 */

/// N64 Controller (NUS-005)
#define JOYBUS_DEVICE_N64_CONTROLLER  (JOYBUS_TYPE_N64_ABSOLUTE | JOYBUS_TYPE_N64_JOYPORT)

/// N64 Mouse (NUS-017)
#define JOYBUS_DEVICE_N64_MOUSE       (JOYBUS_TYPE_N64_RELATIVE)

/// GameCube controller (DOL-003)
#define JOYBUS_DEVICE_GCN_CONTROLLER  (JOYBUS_TYPE_GCN_DEVICE | JOYBUS_TYPE_GCN_STANDARD)

/// WaveBird receiver (DOL-005)
#define JOYBUS_DEVICE_GCN_WAVEBIRD    (JOYBUS_TYPE_GCN_DEVICE | JOYBUS_TYPE_GCN_WIRELESS | JOYBUS_TYPE_GCN_NO_MOTOR)

/**
 * Get the controller type from an "identify" buffer
 *
 * @param id pointer to a 3-byte "identify" buffer
 * @return the controller type value (first two bytes of ID field)
 */
static inline uint16_t joybus_id_get_type(const uint8_t *id)
{
  return (uint16_t)((id[0] << 8) | id[1]);
}

/**
 * Clear type flags in an "identify" buffer
 *
 * @param id pointer to a 3-byte "identify" buffer
 * @param flags type flags to clear
 */
static inline void joybus_id_clear_type_flags(uint8_t *id, uint16_t flags)
{
  id[0] &= ~(flags >> 8);
  id[1] &= ~(flags & 0xFF);
}

/**
 * Set type flags in an "identify" buffer
 *
 * @param id pointer to the 3-byte ID field from an "identify" buffer
 * @param flags type flags to set
 */
static inline void joybus_id_set_type_flags(uint8_t *id, uint16_t flags)
{
  id[0] |= (flags >> 8);
  id[1] |= (flags & 0xFF);
}

/**
 * Get the status byte from an "identify" buffer
 *
 * @param id pointer to the 3-byte ID field from an "identify" buffer
 * @return the status byte (third byte of ID field)
 */
static inline uint8_t joybus_id_get_status(const uint8_t *id)
{
  return id[2];
}

/**
 * Clear status flags in an "identify" buffer
 *
 * @param id pointer to the 3-byte ID field from an "identify" buffer
 * @param flags status flags to clear
 */
static inline void joybus_id_clear_status_flags(uint8_t *id, uint8_t flags)
{
  id[2] &= ~flags;
}

/**
 * Set status flags in an "identify" buffer
 *
 * @param id pointer to the 3-byte ID field from an "identify" buffer
 * @param flags status flags to set
 */
static inline void joybus_id_set_status_flags(uint8_t *id, uint8_t flags)
{
  id[2] |= flags;
}

/**
 * Get the 10-bit wireless ID from an "identify" buffer
 *
 * @param id pointer to the 3-byte ID field from an "identify" buffer
 * @return the wireless ID (bits 9–2 of bytes 1 and 2 of ID field)
 */
static inline uint8_t joybus_id_get_wireless_id(const uint8_t *id)
{
  return (uint16_t)((id[1] & 0xC0) << 2 | id[2]);
}

/**
 * Set the 10-bit wireless ID in an "identify" buffer
 *
 * @param id pointer to the 3-byte ID field from an "identify" buffer
 * @param wireless_id the wireless ID to set (10 bits)
 */
static inline void joybus_id_set_wireless_id(uint8_t *id, uint16_t wireless_id)
{
  id[1] = (id[1] & ~0xC0) | ((wireless_id >> 2) & 0xC0);
  id[2] = wireless_id & 0xFF;
}

/**
 * Check if the accessory changed flag is set in an "identify" buffer
 *
 * @param id pointer to the 3-byte ID field from an "identify" buffer
 * @return true if accessory changed flag is set, false otherwise
 */
static inline bool joybus_id_n64_accessory_changed(uint8_t *id)
{
  return (id[2] & JOYBUS_STATUS_N64_ACCESSORY_PRESENT) && (id[2] & JOYBUS_STATUS_N64_ACCESSORY_PULLED);
}
