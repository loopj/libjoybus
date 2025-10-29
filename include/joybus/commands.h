/**
 * @file commands.h
 *
 * @brief Joybus command codes and utilities
 */
#pragma once

#include <stdint.h>

// Some commands (EEPROM read/write, RTC read/write) are only used by the internal
// N64 joybus line, but we are including them here for completeness.

// Joybus command codes
#define JOYBUS_CMD_RESET                  0xFF    ///< Joybus "reset" command
#define JOYBUS_CMD_IDENTIFY               0x00    ///< Joybus "identify" command
#define JOYBUS_CMD_N64_READ               0x01    ///< N64 controller "read" command
#define JOYBUS_CMD_N64_ACCESSORY_READ     0x02    ///< N64 controller "accessory read" command
#define JOYBUS_CMD_N64_ACCESSORY_WRITE    0x03    ///< N64 controller "accessory write" command
#define JOYBUS_CMD_N64_EEPROM_READ        0x04
#define JOYBUS_CMD_N64_EEPROM_WRITE       0x05
#define JOYBUS_CMD_N64_RTC_INFO           0x06
#define JOYBUS_CMD_N64_RTC_READ           0x07
#define JOYBUS_CMD_N64_RTC_WRITE          0x08
#define JOYBUS_CMD_N64_KEYBOARD_READ      0x13
#define JOYBUS_CMD_GBA_READ               0x14
#define JOYBUS_CMD_GBA_WRITE              0x15
#define JOYBUS_CMD_PIXELFX_GAMEID         0x1D
#define JOYBUS_CMD_GCN_READ               0x40
#define JOYBUS_CMD_GCN_READ_ORIGIN        0x41
#define JOYBUS_CMD_GCN_CALIBRATE          0x42
#define JOYBUS_CMD_GCN_READ_LONG          0x43
#define JOYBUS_CMD_GCN_PROBE_DEVICE       0x4D
#define JOYBUS_CMD_GCN_FIX_DEVICE         0x4E
#define JOYBUS_CMD_GCN_KEYBOARD_READ      0x54

// Joybus command transfer lengths
#define JOYBUS_CMD_RESET_TX               1
#define JOYBUS_CMD_RESET_RX               3
#define JOYBUS_CMD_IDENTIFY_TX            1
#define JOYBUS_CMD_IDENTIFY_RX            3
#define JOYBUS_CMD_N64_READ_TX            1
#define JOYBUS_CMD_N64_READ_RX            4
#define JOYBUS_CMD_N64_ACCESSORY_READ_TX  3
#define JOYBUS_CMD_N64_ACCESSORY_READ_RX  33
#define JOYBUS_CMD_N64_ACCESSORY_WRITE_TX 35
#define JOYBUS_CMD_N64_ACCESSORY_WRITE_RX 1
#define JOYBUS_CMD_N64_EEPROM_READ_TX     2
#define JOYBUS_CMD_N64_EEPROM_READ_RX     8
#define JOYBUS_CMD_N64_EEPROM_WRITE_TX    10
#define JOYBUS_CMD_N64_EEPROM_WRITE_RX    1
#define JOYBUS_CMD_N64_RTC_INFO_TX        1
#define JOYBUS_CMD_N64_RTC_INFO_RX        3
#define JOYBUS_CMD_N64_RTC_READ_TX        2
#define JOYBUS_CMD_N64_RTC_READ_RX        9
#define JOYBUS_CMD_N64_RTC_WRITE_TX       10
#define JOYBUS_CMD_N64_RTC_WRITE_RX       1
#define JOYBUS_CMD_N64_KEYBOARD_READ_TX   2
#define JOYBUS_CMD_N64_KEYBOARD_READ_RX   7
#define JOYBUS_CMD_GBA_READ_TX            3
#define JOYBUS_CMD_GBA_READ_RX            33
#define JOYBUS_CMD_GBA_WRITE_TX           35
#define JOYBUS_CMD_GBA_WRITE_RX           1
#define JOYBUS_CMD_PIXELFX_GAMEID_TX      11
#define JOYBUS_CMD_PIXELFX_GAMEID_RX      0
#define JOYBUS_CMD_GCN_READ_TX            3
#define JOYBUS_CMD_GCN_READ_RX            8
#define JOYBUS_CMD_GCN_READ_ORIGIN_TX     1
#define JOYBUS_CMD_GCN_READ_ORIGIN_RX     10
#define JOYBUS_CMD_GCN_CALIBRATE_TX       3
#define JOYBUS_CMD_GCN_CALIBRATE_RX       10
#define JOYBUS_CMD_GCN_READ_LONG_TX       3
#define JOYBUS_CMD_GCN_READ_LONG_RX       10
#define JOYBUS_CMD_GCN_PROBE_DEVICE_TX    3
#define JOYBUS_CMD_GCN_PROBE_DEVICE_RX    8
#define JOYBUS_CMD_GCN_FIX_DEVICE_TX      3
#define JOYBUS_CMD_GCN_FIX_DEVICE_RX      3
#define JOYBUS_CMD_GCN_KEYBOARD_READ_TX   3
#define JOYBUS_CMD_GCN_KEYBOARD_READ_RX   8

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