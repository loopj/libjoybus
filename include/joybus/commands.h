/**
 * @file commands.h
 *
 * Joybus command codes and transfer lengths.
 */

#pragma once

/**
 * Reset command
 */
#define JOYBUS_CMD_RESET                  0xFF
#define JOYBUS_CMD_RESET_TX               1    ///< Reset command length
#define JOYBUS_CMD_RESET_RX               3    ///< Reset response length

/**
 * Identify command
 */
#define JOYBUS_CMD_IDENTIFY               0x00
#define JOYBUS_CMD_IDENTIFY_TX            1    ///< Identify command length
#define JOYBUS_CMD_IDENTIFY_RX            3    ///< Identify response length

/**
 * N64 read command
 */
#define JOYBUS_CMD_N64_READ               0x01
#define JOYBUS_CMD_N64_READ_TX            1    ///< N64 read command length
#define JOYBUS_CMD_N64_READ_RX            4    ///< N64 read response length

/**
 * N64 accessory read command
 */
#define JOYBUS_CMD_N64_ACCESSORY_READ     0x02
#define JOYBUS_CMD_N64_ACCESSORY_READ_TX  3    ///< N64 accessory read command length
#define JOYBUS_CMD_N64_ACCESSORY_READ_RX  33   ///< N64 accessory read response length

/**
 * N64 accessory write command
 */
#define JOYBUS_CMD_N64_ACCESSORY_WRITE    0x03
#define JOYBUS_CMD_N64_ACCESSORY_WRITE_TX 35   ///< N64 accessory write command length
#define JOYBUS_CMD_N64_ACCESSORY_WRITE_RX 1    ///< N64 accessory write response length

/**
 * N64 EEPROM read command
 */
#define JOYBUS_CMD_N64_EEPROM_READ        0x04
#define JOYBUS_CMD_N64_EEPROM_READ_TX     2    ///< N64 EEPROM read command length
#define JOYBUS_CMD_N64_EEPROM_READ_RX     8    ///< N64 EEPROM read response length

/**
 * N64 EEPROM write command
 */
#define JOYBUS_CMD_N64_EEPROM_WRITE       0x05
#define JOYBUS_CMD_N64_EEPROM_WRITE_TX    10   ///< N64 EEPROM write command length
#define JOYBUS_CMD_N64_EEPROM_WRITE_RX    1    ///< N64 EEPROM write response length

/**
 * N64 RTC info command
 */
#define JOYBUS_CMD_N64_RTC_INFO           0x06
#define JOYBUS_CMD_N64_RTC_INFO_TX        1    ///< N64 RTC info command length
#define JOYBUS_CMD_N64_RTC_INFO_RX        3    ///< N64 RTC info response length

/**
 * N64 RTC read command
 */
#define JOYBUS_CMD_N64_RTC_READ           0x07
#define JOYBUS_CMD_N64_RTC_READ_TX        2    ///< N64 RTC read command length
#define JOYBUS_CMD_N64_RTC_READ_RX        9    ///< N64 RTC read response length

/**
 * N64 RTC write command
 */
#define JOYBUS_CMD_N64_RTC_WRITE          0x08
#define JOYBUS_CMD_N64_RTC_WRITE_TX       10   ///< N64 RTC write command length
#define JOYBUS_CMD_N64_RTC_WRITE_RX       1    ///< N64 RTC write response length

/**
 * N64 VRU read36 command
 */
#define JOYBUS_CMD_N64_VRU_READ36         0x09
#define JOYBUS_CMD_N64_VRU_READ36_TX      3    ///< N64 VRU read36 command length
#define JOYBUS_CMD_N64_VRU_READ36_RX      37   ///< N64 VRU read36 response length

/**
 * N64 VRU write20 command
 */
#define JOYBUS_CMD_N64_VRU_WRITE20        0x0A
#define JOYBUS_CMD_N64_VRU_WRITE20_TX     23   ///< N64 VRU write20 command length
#define JOYBUS_CMD_N64_VRU_WRITE20_RX     1    ///< N64 VRU write20 response length

/**
 * N64 VRU read2 command
 */
#define JOYBUS_CMD_N64_VRU_READ2          0x0B
#define JOYBUS_CMD_N64_VRU_READ2_TX       3    ///< N64 VRU read2 command length
#define JOYBUS_CMD_N64_VRU_READ2_RX       3    ///< N64 VRU read2 response length

/**
 * N64 VRU write4 command
 */
#define JOYBUS_CMD_N64_VRU_WRITE4         0x0C
#define JOYBUS_CMD_N64_VRU_WRITE4_TX      7    ///< N64 VRU write4 command length
#define JOYBUS_CMD_N64_VRU_WRITE4_RX      1    ///< N64 VRU write4 response length

/**
 * N64 VRU swrite command
 */
#define JOYBUS_CMD_N64_VRU_SWRITE         0x0D
#define JOYBUS_CMD_N64_VRU_SWRITE_TX      3    ///< N64 VRU swrite command length
#define JOYBUS_CMD_N64_VRU_SWRITE_RX      1    ///< N64 VRU swrite response length

/**
 * N64 keyboard read command
 */
#define JOYBUS_CMD_N64_KEYBOARD_READ      0x13
#define JOYBUS_CMD_N64_KEYBOARD_READ_TX   2    ///< N64 keyboard read command length
#define JOYBUS_CMD_N64_KEYBOARD_READ_RX   7    ///< N64 keyboard read response length

/**
 * GBA read command
 */
#define JOYBUS_CMD_GBA_READ               0x14
#define JOYBUS_CMD_GBA_READ_TX            3    ///< GBA read command length
#define JOYBUS_CMD_GBA_READ_RX            33   ///< GBA read response length

/**
 * GBA write command
 */
#define JOYBUS_CMD_GBA_WRITE              0x15
#define JOYBUS_CMD_GBA_WRITE_TX           35   ///< GBA write command length
#define JOYBUS_CMD_GBA_WRITE_RX           1    ///< GBA write response length

/**
 * PixelFX game ID command
 */
#define JOYBUS_CMD_PIXELFX_GAMEID         0x1D
#define JOYBUS_CMD_PIXELFX_GAMEID_TX      11   ///< PixelFX game ID command length
#define JOYBUS_CMD_PIXELFX_GAMEID_RX      0    ///< PixelFX game ID response length

/**
 * GCN read command
 */
#define JOYBUS_CMD_GCN_READ               0x40
#define JOYBUS_CMD_GCN_READ_TX            3    ///< GCN read command length
#define JOYBUS_CMD_GCN_READ_RX            8    ///< GCN read response length

/**
 * GCN read origin command
 */
#define JOYBUS_CMD_GCN_READ_ORIGIN        0x41
#define JOYBUS_CMD_GCN_READ_ORIGIN_TX     1    ///< GCN read origin command length
#define JOYBUS_CMD_GCN_READ_ORIGIN_RX     10   ///< GCN read origin response length

/**
 * GCN calibrate command
 */
#define JOYBUS_CMD_GCN_CALIBRATE          0x42
#define JOYBUS_CMD_GCN_CALIBRATE_TX       3    ///< GCN calibrate command length
#define JOYBUS_CMD_GCN_CALIBRATE_RX       10   ///< GCN calibrate response length

/**
 * GCN read long command
 */
#define JOYBUS_CMD_GCN_READ_LONG          0x43
#define JOYBUS_CMD_GCN_READ_LONG_TX       3    ///< GCN read long command length
#define JOYBUS_CMD_GCN_READ_LONG_RX       10   ///< GCN read long response length

/**
 * GCN probe device command
 */
#define JOYBUS_CMD_GCN_PROBE_DEVICE       0x4D
#define JOYBUS_CMD_GCN_PROBE_DEVICE_TX    3    ///< GCN probe device command length
#define JOYBUS_CMD_GCN_PROBE_DEVICE_RX    8    ///< GCN probe device response length

/**
 * GCN fix device command
 */
#define JOYBUS_CMD_GCN_FIX_DEVICE         0x4E
#define JOYBUS_CMD_GCN_FIX_DEVICE_TX      3    ///< GCN fix device command length
#define JOYBUS_CMD_GCN_FIX_DEVICE_RX      3    ///< GCN fix device response length

/**
 * GCN keyboard read command
 */
#define JOYBUS_CMD_GCN_KEYBOARD_READ      0x54
#define JOYBUS_CMD_GCN_KEYBOARD_READ_TX   3    ///< GCN keyboard read command length
#define JOYBUS_CMD_GCN_KEYBOARD_READ_RX   8    ///< GCN keyboard read response length
