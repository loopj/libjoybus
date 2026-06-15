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
 * The first two bytes of the identify response contain the device type flags.
 *
 * The following flags map to the little-endian representation of the first two
 * bytes of the identify response. We're using a little-endian representation,
 * since modern microcontrollers are pretty much all little-endian by default.
 * This helps us avoid byte swapping when parsing the identify response, we can
 * cast the bytes to a joybus_id struct and use these masks as-is.
 */

// Non-GameCube device types
#define JOYBUS_TYPE_N64_MASK                  0x1f07  ///< N64 device type mask
#define JOYBUS_TYPE_N64_ABSOLUTE              0x0001  ///< N64 device reports absolute position
#define JOYBUS_TYPE_N64_RELATIVE              0x0002  ///< N64 device reports relative position
#define JOYBUS_TYPE_N64_JOYPORT               0x0004  ///< N64 device has a Joyport (pak port)
#define JOYBUS_TYPE_N64_VRU                   0x0100  ///< N64 Voice Recognition Unit (NUS-020)
#define JOYBUS_TYPE_N64_KEYBOARD              0x0200  ///< N64 Randnet Keyboard (RND-001)
#define JOYBUS_TYPE_GBA_CABLE                 0x0400  ///< GBA connected via GameCube Game Boy Advance cable (DOL-011)
#define JOYBUS_TYPE_N64_RTC                   0x1000  ///< N64 cartridge RTC is present
#define JOYBUS_TYPE_N64_EEPROM_16K            0x4000  ///< N64 cartridge EEPROM is 16KB (otherwise 4KB)
#define JOYBUS_TYPE_N64_EEPROM                0x8000  ///< N64 cartridge EEPROM is present

// GameCube device types
#define JOYBUS_TYPE_GCN_MASK                  0x0018  ///< GameCube device type mask
#define JOYBUS_TYPE_GCN_STANDARD              0x0001  ///< Standard GameCube controller
#define JOYBUS_TYPE_GCN_WIRELESS_STATE        0x0002  ///< Wireless state available
#define JOYBUS_TYPE_GCN_DEVICE                0x0008  ///< Device is a GameCube device
#define JOYBUS_TYPE_GCN_NO_MOTOR              0x0020  ///< No rumble motor present
#define JOYBUS_TYPE_GCN_WIRELESS_RECEIVED     0x0040  ///< Wireless receiver has received a packet
#define JOYBUS_TYPE_GCN_WIRELESS              0x0080  ///< Controller is wireless
#define JOYBUS_TYPE_GCN_WIRELESS_ID_FIXED     0x1000  ///< Wireless ID has been fixed
#define JOYBUS_TYPE_GCN_KEYBOARD              0x2000  ///< GameCube ASCII keyboard
#define JOYBUS_TYPE_GCN_WIRELESS_ORIGIN       0x2000  ///< Wireless origin available
#define JOYBUS_TYPE_GCN_WIRELESS_ID_MASK      0xC000  ///< Top 2 bits of wireless ID

/*
 * Joybus device status flags.
 *
 * The last byte of the identify response contains the device "status" flags.
 */

// Status flags for N64 controllers with a Joyport
#define JOYBUS_STATUS_N64_PAK_PRESENT         0x01    ///< Pak present
#define JOYBUS_STATUS_N64_PAK_PULLED          0x02    ///< Pak removal/change detected
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
 * Represents the 3-byte ID field from an "identify" buffer
 */
struct joybus_id {
  /// Type flags
  uint16_t type;

  /// Status flags
  uint8_t status;
} __attribute__((packed));

/**
 * Clear type flags in an "identify" buffer
 *
 * @param id the id to clear type flags from
 * @param flags type flags to clear
 */
static inline void joybus_id_clear_type_flags(struct joybus_id *id, uint16_t flags)
{
  id->type &= ~flags;
}

/**
 * Set type flags in an "identify" buffer
 *
 * @param id the id to set type flags on
 * @param flags type flags to set
 */
static inline void joybus_id_set_type_flags(struct joybus_id *id, uint16_t flags)
{
  id->type |= flags;
}

/**
 * Clear status flags in an "identify" buffer
 *
 * @param id the id to clear status flags from
 * @param flags status flags to clear
 */
static inline void joybus_id_clear_status_flags(struct joybus_id *id, uint8_t flags)
{
  id->status &= ~flags;
}

/**
 * Set status flags in an "identify" buffer
 *
 * @param id the id to set status flags on
 * @param flags status flags to set
 */
static inline void joybus_id_set_status_flags(struct joybus_id *id, uint8_t flags)
{
  id->status |= flags;
}

/**
 * Get the 10-bit wireless ID from an "identify" buffer
 *
 * @param id the id to get the wireless ID from
 * @return the wireless ID (bits 9–2 of bytes 1 and 2 of ID field)
 */
static inline uint16_t joybus_id_get_wireless_id(const struct joybus_id *id)
{
  return (uint16_t)((id->type & 0xC000) >> 6 | id->status);
}

/**
 * Set the 10-bit wireless ID in an "identify" buffer
 *
 * @param id the id to set the wireless ID on
 * @param wireless_id the wireless ID to set (10 bits)
 */
static inline void joybus_id_set_wireless_id(struct joybus_id *id, uint16_t wireless_id)
{
  id->type   = (id->type & ~0xC000) | ((uint16_t)((wireless_id >> 2) & 0xC0) << 8);
  id->status = wireless_id & 0xFF;
}

/**
 * Check if the pak changed flag is set in an "identify" buffer
 *
 * @param id the id to check the pak changed flag on
 * @return true if pak changed flag is set, false otherwise
 */
static inline bool joybus_id_n64_pak_changed(const struct joybus_id *id)
{
  return (id->status & JOYBUS_STATUS_N64_PAK_PRESENT) && (id->status & JOYBUS_STATUS_N64_PAK_PULLED);
}
