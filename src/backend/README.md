# Backend Implementations

A backend adapts libjoybus to a specific microcontroller family. This document is the source of truth for backend behavior. It covers the structure of a backend, the behavior required in host mode and target mode, and the OEM device timings those requirements are derived from.

## Structure

New backends should follow the structure of the existing backends in this directory.

Each backend defines a `struct joybus_<backend>` holding a `struct joybus base` and the backend's own state, a `joybus_<backend>_config` struct holding the constant configuration for a bus (peripherals, transmit frequency), a `joybus_<backend>_config_default(...)` helper that fills it with sensible defaults, and a `joybus_<backend>_init(bus, config)` function. It also provides implementations for each of the functions in the `joybus_api` struct:

```c
static const struct joybus_api mybackend_api = {
  .enable   = joybus_mybackend_enable,
  .disable  = joybus_mybackend_disable,
  .transfer = joybus_mybackend_transfer,
};

int joybus_mybackend_init(struct joybus_mybackend *mybackend_bus, struct joybus_mybackend_config config)
{
  // Save the bus API and common configuration
  struct joybus *bus = JOYBUS(mybackend_bus);
  bus->api           = &mybackend_api;
  bus->freq          = config.freq;
  bus->targets       = NULL;
  bus->active_target = NULL;

  // Rest of initialization code...

  return 0;
}
```

## Peripherals

Since backends need to clock in and out pulses on the bus with microsecond precision, bit-banging is typically not feasible. Using dedicated hardware peripherals which can capture and generate signals with minimal CPU intervention is recommended.

Some examples of approaches:

- Use a timer peripheral in capture mode to fill a buffer with edge timings. Once enough data has been collected, measure pulse widths in software.
- Use a timer peripheral in PWM mode to clock out pulses.
- Use a serial communication peripheral (e.g. SPI, USART) to clock out pulses after encoding them into the suitable line coding.
- Some platforms (e.g. RP2040) even have programmable IO peripherals which can "bit-bang" the protocol completely in hardware.

The turnaround times between transmitting and receiving data are also critical. Your backend must be able to switch from transmit to receive mode immediately in order to capture the response.

## Host-mode Transfers

- Transfers must be async / interrupt-driven
- Transfers must return `-JOYBUS_ERR_DISABLED` if the bus is not in host mode
- Transfers must return `-JOYBUS_ERR_BUSY` if the bus is not idle
- Hosts are assumed to own the bus, so don't wait for bus-idle before initiating a transfer
- Transfer callbacks must be called as soon as last byte of response is received, and the bus must be returned to idle state
- Since users may choose to trigger another transfer from the callback, we must enforce an inter-transfer delay. The minimum is a property of the attached device rather than the protocol - measured devices range from ~7us for a GameCube controller to ~80us for an N64 controller, and a GameCube GBA cable needs ~64us. Use 80us (`JOYBUS_INTER_TRANSFER_DELAY_US`) unless the device is known to tolerate less
- The backend must be ready to clock in a response within 3us from sending the falling edge of the last data bit (see measured timings below)
- Enforce that the first byte of a response is received within 64us (`JOYBUS_REPLY_TIMEOUT_US`) of the falling edge of the last data bit, otherwise call the callback with an error
- If we are expecting more bytes, they must be contiguous, otherwise call the callback with an error - timeout mechanism can be determined by the backend (per bit, per byte, etc)
- Inter-transfer delay should apply to successful and errored transfers

## Target-mode Command Reception

- When enabled, target mode should await bus idle before receiving a command. The bus is idle once the line has been continuously high for 100us (`JOYBUS_BUS_IDLE_US`)
- A falling edge on the bus indicates the start of a command
- Received bytes are streamed to the targets: `joybus_byte_received` should be called as soon as possible after each complete byte is received. It offers the first byte of a command to each attached target in attachment order, and routes every later byte to the target that claimed the command
- After the first byte is received, if more bytes are expected, they should be received contiguously, otherwise wait for the next command (await bus idle) - timeout mechanism can be determined by the backend (per bit, per byte, etc)
- If `joybus_byte_received` returns an error, including when no target is attached or no attached target recognizes the command's opcode, ignore the command and wait for the next command (await bus idle)
- The target handling a command can call the `joybus_target_response_cb` at any time to signal to the backend that response bytes are available
- Since the `joybus_target_response_cb` can be called before the last byte, we can use this as an opportunity to begin pre-encoding the response while waiting for the remaining bytes to be clocked in
- Once the final byte of a command is received, we must begin clocking out the response IMMEDIATELY (OEM devices turn around in 3.1 - 4.4us, or 6.6 - 7.0us for a GBA cable, see below for measured OEM timings)

## Reducing Turnaround

The hard deadline in both modes is the turnaround between the last bit of a command and the first bit of the reply. These are the techniques the existing backends use to meet it.

- Stream command bytes to the target as they arrive. `joybus_byte_received` is called once per byte rather than once per command, so a target can recognize a command from its opcode and call `joybus_target_response_cb` before the final byte has been clocked in. Every technique below depends on the backend holding the response before the command ends.
- Pre-encode the response into the wire format while the remaining bits arrive. The gecko backend encodes the first one or two bytes in `prepare_write` and encodes each later byte from the transmit DMA interrupt, so encoding never blocks the wire.
- Arm the transmitter early and gate it, so starting the reply costs a single write. The gecko backend starts its transmit DMA with the peripheral request disabled (`REQDIS_SET`) and releases it with `REQDIS_CLR` once the last byte lands. The rp2xxx backend starts its transmit DMA as soon as the response arrives, which fills the PIO transmit FIFO while the state machine is still in its receive loop, then switches with a single `jmp`.
- Start transmitting before the whole response is encoded. The esp32 backend writes the first symbol of the first byte, starts the transmitter immediately, and fills the remaining symbols behind the hardware as it reads them.
- Arm the receiver before the reply can arrive, never in response to a transmit-complete interrupt. The esp32 backend arms RX before it transmits and captures the command, the stop bit, and the reply as one continuous capture, then decodes the reply from a fixed symbol offset. This removes interrupt latency from the path and works regardless of MCU clock speed.
- Let the hardware perform the turnaround where the peripheral allows it. The rp2xxx host PIO program sends the stop bit, fires its interrupt, and falls straight into its receive loop, so no CPU work sits between the end of the command and being ready to sample the reply.
- Fire the byte-received interrupt early and decode underneath the bits still arriving. The esp32 backend sets the RMT receive limit a symbol or two short of a full byte, folds in each bit as its symbol commits, and busy-waits only for the last one. Interrupt latency is hidden behind the tail of the byte instead of being added after it.
- Do setup work while the bus is idle. The esp32 backend resets its transmit pointer when it enters receive mode, keeping that work off the critical path.
- Keep the hot path out of flash. The esp32 backend marks its encode, decode, and interrupt handlers `IRAM_ATTR`. Running the target handler and checksum from flash measurably slows the reply on parts that fetch instructions from external flash.

Replying faster than an OEM device has no known benefit. The esp32 backend enforces a floor (`TARGET_REPLY_FLOOR_NS`) so it never replies much sooner than the measured OEM turnaround below.

## Checklist

When implementing a new backend, the following requirements are considered the bare minimum:

- [ ] Backend implements all functions in `joybus_api`
- [ ] Backend meets every host-mode and target-mode requirement above

Ideally, your backend should also meet the following criteria:

- [ ] Backend supports 4 simultaneous buses

## Nominal Device Timings

Every device derives its bus frequency from its own clock, so the frequency on the wire varies by device. The nominal Joybus frequency is 250 kHz. These values are defined in [`include/joybus/bus.h`](../../include/joybus/bus.h).

| Device | Frequency | Pulse period | Clock source |
| --- | --- | --- | --- |
| N64 console (NUS-001) | 244.14 kHz | 4.10 us | PIF-NUS @ 15.625MHz / 64 |
| N64 cartridge EEPROM/RTC | 244.14 kHz | 4.10 us | SECCLK @ 1.953125MHz / 8 (PIF-NUS / 8) |
| N64 controller (NUS-005) | 250.00 kHz | 4.00 us | CNT-NUS @ 2MHz / 8 |
| N64 VRU (NUS-020) | 250.00 kHz | 4.00 us | VCI-NUS @ 4MHz / 16 |
| GameCube console (DOL-001 / DOL-101) | 202.50 kHz | 4.94 us | Flipper @ 162MHz / 800 |
| GameCube controller (DOL-003) | 250.00 kHz | 4.00 us | CNT-DOL @ 4MHz / 16 |
| WaveBird receiver (DOL-005) | 225.00 kHz | 4.44 us | WCRX-DOL @ 28.8MHz / 128 |
| Wii console (RVL-001 / RVL-101 / RVL-201) | 202.50 kHz | 4.94 us | Hollywood @ 243MHz / 1200 |
| GBA cable (DOL-011) | 262.14 kHz | 3.81 us | CPU-AGB @ 16.777216MHz / 64 |

## Measured Timings

### OEM N64 Controller

```
Device ID:                            05 00 01
Response frequency:                   253.16 kHz (3.95 us pulse period)
Minimum tolerated inter-command gap:  ~79.60 us
Accepted command frequency range:     10.00 - 485.00 kHz

Turnaround (us, 100 samples)
  command             first   min     mean    max     stdev
  identify (0x00)     3.55    3.15    3.23    3.63    0.16
  reset (0xFF)        3.50    3.15    3.22    3.63    0.16
  n64 read (0x01)     3.36    3.25    3.27    3.62    0.07
```

### OEM GameCube Controller

```
Device ID:                            09 00 03
Response frequency:                   251.57 kHz (3.98 us pulse period)
Minimum tolerated inter-command gap:  ~7.27 us
Accepted command frequency range:     95.00 - 445.00 kHz

Turnaround (us, 100 samples)
  command             first   min     mean    max     stdev
  identify (0x00)     3.55    3.35    3.43    3.71    0.06
  reset (0xFF)        3.26    3.26    3.42    3.68    0.06
  gcn read (0x40)     3.51    3.23    3.50    3.68    0.04
  gcn origin (0x41)   3.71    3.27    3.53    3.71    0.08
```

### OEM WaveBird Receiver

```
Device ID:                            A8 00 00
Response frequency:                   225.37 kHz (4.44 us pulse period)
Minimum tolerated inter-command gap:  ~8.97 us
Accepted command frequency range:     180.00 - 300.00 kHz

Turnaround (us, 100 samples)
  command             first   min     mean    max     stdev
  identify (0x00)     4.11    4.05    4.07    4.37    0.03
  reset (0xFF)        4.12    3.98    4.06    4.12    0.08
  gcn read (0x40)     4.02    3.87    3.90    4.32    0.10
  gcn origin (0x41)   4.06    3.85    3.87    4.06    0.08
```

### OEM GBA to GameCube Cable

```
Device ID:                            00 04 28
Response frequency:                   262.32 kHz (3.81 us pulse period)
Minimum tolerated inter-command gap:  ~63.95 us
Accepted command frequency range:     6.00 - 322.00 kHz

Turnaround (us, 100 samples)
  command             first   min     mean    max     stdev
  identify (0x00)     6.72    6.56    6.58    6.72    0.09
  reset (0xFF)        6.96    6.55    6.59    6.96    0.12
```

### OEM N64 VRU

```
Device ID:                            00 01 00
Response frequency:                   250.81 kHz (3.99 us pulse period)
Minimum tolerated inter-command gap:  ~79.82 us
Accepted command frequency range:     5.00 - 505.00 kHz

Turnaround (us, 100 samples)
  command             first   min     mean    max     stdev
  identify (0x00)     3.43    3.12    3.14    3.55    0.08
  reset (0xFF)        3.12    3.12    3.13    3.15    0.04
```
