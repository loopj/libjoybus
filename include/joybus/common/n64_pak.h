/**
 * @file
 *
 * Common definitions for N64 paks.
 */

#pragma once

/// Start of the probe area, where a console detects a pak's type and selects a bank
#define JOYBUS_N64_PAK_PROBE_ADDR 0x8000

/// Start of the motor area of a Rumble Pak
#define JOYBUS_N64_PAK_MOTOR_ADDR 0xC000
