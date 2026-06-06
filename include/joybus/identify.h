/**
 * @file identify.h
 *
 * @brief Joybus "identify" type/status values and utilities
 */
#pragma once

#include <stdint.h>

// N64 identify type values
#define JOYBUS_ID_N64_VRU                 0x0001  ///< Voice Recognition Unit (NUS-020)
#define JOYBUS_ID_N64_KEYBOARD            0x0002  ///< Randnet Keyboard (RND-001)
#define JOYBUS_ID_N64_GBA_LINK            0x0004  ///< GBA Link Cable (DOL-011)
#define JOYBUS_ID_N64_MOUSE               0x0200  ///< N64 Mouse (NUS-017)
#define JOYBUS_ID_N64_CONTROLLER          0x0500  ///< Standard N64 Controller (NUS-005)

// N64 identify status values (standard controllers)
#define JOYBUS_ID_N64_ACCESSORY_PRESENT   0x01    ///< Accessory present
#define JOYBUS_ID_N64_ACCESSORY_ABSENT    0x02    ///< Accessory absent
#define JOYBUS_ID_N64_ACCESSORY_CHANGED   0x03    ///< Accessory changed
#define JOYBUS_ID_N64_CHECKSUM_ERROR      0x04    ///< Transfer checksum error

// N64 identify status values (VRU)
#define JOYBUS_ID_N64_VRU_UNINITIALIZED   0x00    ///< VRU is uninitialized
#define JOYBUS_ID_N64_VRU_INITIALIZED     0x01    ///< VRU is initialized

// GCN identify type flags
#define JOYBUS_ID_GCN_WIRELESS_ID_FIXED   0x0010  ///< Wireless ID has been fixed
#define JOYBUS_ID_GCN_WIRELESS_ORIGIN     0x0020  ///< Wireless origin available
#define JOYBUS_ID_GCN_STANDARD            0x0100  ///< Standard GameCube controller
#define JOYBUS_ID_GCN_WIRELESS_STATE      0x0200  ///< Wireless state available
#define JOYBUS_ID_GCN_DEVICE              0x0800  ///< Device is a GameCube device
#define JOYBUS_ID_GCN_NO_MOTOR            0x2000  ///< No rumble motor present
#define JOYBUS_ID_GCN_WIRELESS_RECEIVED   0x4000  ///< Wireless receiver has received a packet
#define JOYBUS_ID_GCN_WIRELESS            0x8000  ///< Controller is wireless

// GCN identify status flags (non-wireless controllers)
#define JOYBUS_ID_GCN_ANALOG_MODE_MASK    0x07    ///< Bits 2–0: Last analog mode
#define JOYBUS_ID_GCN_MOTOR_STATE_MASK    0x18    ///< Bits 4–3: Last motor state
#define JOYBUS_ID_GCN_NEED_ORIGIN         0x20    ///< New origin data available (host should read origin)
#define JOYBUS_ID_GCN_ERROR_LATCHED       0x40    ///< Latched error
#define JOYBUS_ID_GCN_ERROR               0x80    ///< Last error
#define JOYBUS_ID_GCN_MOTOR_STATE_SHIFT   3
#define JOYBUS_ID_GCN_ANALOG_MODE_SHIFT   0

/// Device type for a standard GameCube controller
#define JOYBUS_GAMECUBE_CONTROLLER        (JOYBUS_ID_GCN_DEVICE | JOYBUS_ID_GCN_STANDARD)

/// Device type for a WaveBird receiver
#define JOYBUS_WAVEBIRD_RECEIVER          (JOYBUS_ID_GCN_DEVICE | JOYBUS_ID_GCN_WIRELESS | JOYBUS_ID_GCN_NO_MOTOR)

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
