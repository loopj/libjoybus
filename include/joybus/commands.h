/**
 * @file commands.h
 *
 * @brief Joybus command codes and transfer lengths.
 */

#pragma once

// Joybus command codes
#define JOYBUS_CMD_RESET                  0xFF
#define JOYBUS_CMD_IDENTIFY               0x00
#define JOYBUS_CMD_N64_READ               0x01
#define JOYBUS_CMD_N64_ACCESSORY_READ     0x02
#define JOYBUS_CMD_N64_ACCESSORY_WRITE    0x03
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