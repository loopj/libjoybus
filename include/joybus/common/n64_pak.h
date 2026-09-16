/**
 * @file
 *
 * Common definitions for N64 paks.
 */

#pragma once

/// Size of a pak read or write block in bytes
#define JOYBUS_N64_PAK_BLOCK_SIZE 32

/// Size of one bank in bytes, the storage of an original Controller Pak
#define JOYBUS_N64_PAK_BANK_SIZE 32768

/// Start of the probe area, where a console detects a pak's type and selects a bank
#define JOYBUS_N64_PAK_PROBE_ADDR 0x8000

/// Start of the motor area of a Rumble Pak
#define JOYBUS_N64_PAK_MOTOR_ADDR 0xC000
