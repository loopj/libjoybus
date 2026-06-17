# Joybus Protocol

```{toctree}
:numbered:
:maxdepth: 3
```

## Overview

The Joybus protocol is a single-wire, self-clocked, half-duplex serial protocol used to connect a console to its controllers and peripherals. It is used by the Nintendo 64, GameCube, and Wii and their accessories (controllers, cartridge peripherals, and similar devices).

This document specifies the protocol's electrical interface, bit-level signalling, symbol encoding, byte framing, the Host-initiated Command/Response Transaction model, and the set of official commands (Section 7).

## Terms and Definitions

| Term | Definition |
|------|------------|
| **Host** | The device that initiates every Transaction by issuing a Command. Typically a game console (e.g. N64, GameCube, Wii). |
| **Target** | The device that responds to Commands. Never initiates a Transaction. Typically a controller or other peripheral connected to a console (e.g. N64 controller, GameCube controller, N64 cartridge). |
| **Transaction** | One complete exchange: a Command from the Host, optionally followed by a Response from the Target. |
| **Command** | A transmission from Host to Target, beginning with an opcode byte. |
| **Response** | A transmission from Target to Host, sent in reply to a Command. |
| **Transmitter** | The device currently driving the line: the Host during a Command, the Target during a Response. |
| **Receiver** | The device currently decoding the line: the Target during a Command, the Host during a Response. |

## Electrical

### Signal line

The bus consists of a single bidirectional data line. There is no clock line; timing is recovered from transitions on the data line itself (see Section 4.3).

### Line states

The line is **open-drain** with a pull-up. The idle state is logic high. A device signals by driving the line low; when no device drives the line, the pull-up returns it to high. Only one device drives the line low at a time; the Host and Target never transmit simultaneously.

### Electrical Characteristics

| Parameter | Symbol | Min | Typ | Max | Notes |
|-----------|--------|-----|-----|-----|-------|
| Bus voltage | V_CC | — | 3.3 V | — | — |
| Pull-up (each end) | R_PU | — | 750 Ω | — | — |

## Bus Timing

### Bit time and data rate

Each bit occupies a nominal **bit time of T_bit = 4 µs**; the corresponding data rate is **250 kbit/s** (the number of data bits per second, not the rate of any underlying clock signal).

T_bit is not fixed: it varies per device, so a Host and Target on the same bus need not match (see Appendix C).

| Parameter | Symbol | Min | Typ | Max | Notes |
|-----------|--------|-----|-----|-----|-------|
| Data rate | f_bit | `[TODO]` | 250 kbit/s | `[TODO]` | Per-device; see Appendix C |
| Bit time | T_bit | `[TODO]` | 4 µs | `[TODO]` | = 1 / f_bit |

### Symbol encoding

Each data bit spans one bit time, divided into four quarter-bit units (T_q), and begins with a falling edge. The two symbols differ only in how long the line is held low:

| Parameter | Value |
|-----------|-------|
| Quarter-bit unit | T_q = T_bit / 4 |
| Logic 0 | 3 T_q low + 1 T_q high |
| Logic 1 | 1 T_q low + 3 T_q high |

### Sampling

There is no clock line. The Receiver times its sample from each bit's falling edge, sampling the line once partway through the bit. Between 1 T_q and 3 T_q after the falling edge the two symbols are distinguishable — a logic 1 has already gone high while a logic 0 is still low — so any sample point in that window decodes the bit. Sampling near the bit midpoint (around 1.5–2 T_q) is recommended.

## Byte and Frame Structure

### Bit order

Data is transmitted **most-significant-bit first** (MSB first).

### Byte structure and inter-byte spacing

A byte is transmitted as eight contiguous data symbols, with no inter-byte gap: bytes are fully contiguous

### Stop bits

A transmission is terminated by a stop bit: the Transmitter drives the line low for a defined number of quarter-bit units and then releases it, after which the pull-up returns the line to idle high. The two stop bits are:

| Parameter | Value |
|-----------|-------|
| Host stop bit | 1 T_q low, then release to high |
| Target stop bit | 2 T_q low, then release to high |

## Transaction Layer

### Roles

There is exactly one Host and one Target. Neither is addressed; the protocol is strictly point-to-point. The Host initiates every Transaction by issuing a Command. The Target transmits only in reply to a Command.

### Command structure

A Command is sent by the Host and begins with a mandatory **opcode** (command ID) byte, optionally followed by argument bytes. The opcode determines both the Command length and the expected Response length. A Command is terminated by a Host stop bit (Section 5.3).

The opcodes supported by official Hosts and Targets, and their Command/Response lengths, are defined in Section 7.

### Response structure

A Response is sent by the Target in reply to a Command and consists of the Response bytes for that opcode, terminated by a Target stop bit (Section 5.3).

A Target that receives a Command with an opcode it does not support sends no Response and leaves the line idle.

### Response timing

After releasing the line at the end of the Host stop bit, the Host expects the Target's Response to begin within **64 µs** (a hardware limit).

In practice, real Targets respond much faster — typically ~2–4 µs for most commands.

## Command Set

The following table lists commands supported by official Nintendo Hosts and Targets.

| Opcode | Name | TX Bytes | RX Bytes | Description |
|:-------|:-----|---------------:|----------------:|-------------|
| 0xFF | Reset | 1 | 3 | Request a reset and identify the device |
| 0x00 | Identify | 1 | 3 | Identify a device |
| 0x01 | N64 Read | 1 | 4 | Read an N64 controller input state |
| 0x02 | N64 Pak Read | 3 | 33 | Read a block of data from a N64 controller Pak |
| 0x03 | N64 Pak Write | 35 | 1 | Write a block of data to a N64 controller Pak |
| 0x04 | N64 EEPROM Read | 2 | 8 | Read a block of data from an N64 cartridge EEPROM |
| 0x05 | N64 EEPROM Write | 10 | 1 | Write a block of data to an N64 cartridge EEPROM |
| 0x06 | N64 RTC Info | 1 | 3 | Read id from an N64 cartridge RTC |
| 0x07 | N64 RTC Read | 2 | 9 | Read a block of data from an N64 cartridge RTC |
| 0x08 | N64 RTC Write | 10 | 1 | Write a block of data to an N64 cartridge RTC |
| 0x09 | N64 VRU Read36 | 3 | 37 | Read a block of data from an N64 VRU |
| 0x0A | N64 VRU Write20 | 23 | 1 | Write a block of data to an N64 VRU |
| 0x0B | N64 VRU Read2 | 3 | 3 | Read a block of data from an N64 VRU |
| 0x0C | N64 VRU Write4 | 7 | 1 | Write a block of data to an N64 VRU |
| 0x0D | N64 VRU Swrite | 3 | 1 | Write a block of data to an N64 VRU ADC |
| 0x13 | N64 Keyboard Read | 2 | 7 | Read the state of an N64 keyboard |
| 0x14 | GBA Read | 3 | 33 | Read a block of data from a Game Boy Advance |
| 0x15 | GBA Write | 35 | 1 | Write a block of data to a Game Boy Advance |
| 0x30 | GCN Wheel Rumble | 3 | 0 | Write a rumble command to a Logitech Speed Force steering wheel |
| 0x40 | GCN Read | 3 | 8 | Read a GCN controller input state |
| 0x41 | GCN Read Origin | 1 | 10 | Read a GameCube controller origin state |
| 0x42 | GCN Recalibrate | 3 | 10 | Recalibrate a GameCube controller, returning the new origin |
| 0x43 | GCN Read Long | 3 | 10 | Read the full-precision GameCube controller state |
| 0x4D | GCN Wireless Probe | 3 | 8 | Probe the state of a wireless GameCube wireless controller |
| 0x4E | GCN Wirless Fix | 3 | 3 | Fix the wireless ID of a wireless GameCube controller |
| 0x54 | GCN Keyboard Read | 3 | 8 | Read the state of a GameCube keyboard |

## Appendix A. Timing Diagrams

`[TODO]` — Add waveform diagrams for logic 0, logic 1, Host stop bit, Target stop bit, and a full Transaction.

## Appendix B. Worked Transaction Example

`[TODO]` — Add a byte-by-byte example of one Command and its Response, with annotated edges and sample points.

## Appendix C. Measured Device Rates and Clock Sources

The following rates apply to official devices, together with the clock source and divisor from which each is derived.

| Device | Model | Joybus frequency (Hz) | Clock source | Divisor |
|--------|-------|----------------------:|--------------|---------|
| N64 console | NUS-001 | 244,141 | PIF-NUS @ 15.625 MHz | ÷ 64 |
| N64 external Joybus device (cart EEPROM/RTC) | — | 244,141 | SECCLK @ 1.953125 MHz | ÷ 8 |
| N64 controller | NUS-005 | 250,000 | CNT-NUS @ 2 MHz | ÷ 8 |
| N64 VRU | NUS-020 | 250,000 | VCI-NUS @ 4 MHz | ÷ 16 |
| GameCube console | DOL-001 / DOL-101 | 202,500 | Flipper @ 162 MHz | ÷ 800 |
| GameCube controller | DOL-003 | 250,000 | CNT-DOL @ 4 MHz | ÷ 16 |
| WaveBird receiver | DOL-005 | 225,000 | WCRX-DOL @ 28.8 MHz | ÷ 128 |
| Wii console | RVL-001 / RVL-101 / RVL-201 | 202,500 | Hollywood @ 243 MHz | ÷ 1200 |
| GameCube–Game Boy Advance cable | DOL-011 | 262,144 | CPU-AGB @ 16.777216 MHz | ÷ 64 |
