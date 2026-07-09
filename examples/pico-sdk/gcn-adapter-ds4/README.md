# Simple GameCube USB Adapter Example

Implements a single port GameCube controller to USB adapter that presents itself
as a Sony DualShock 4 (DS4) controller.


## Rumble (OS-native on macOS)

The adapter emulates a DualShock 4 (`054C:05C4`), a controller macOS recognizes
natively through its GameController framework. Because of this, rumble works with
no extra software: games, Steam, and the OS drive the DS4 rumble output report
(`0x05`) directly, and the adapter forwards it to the GameCube controller's motor.

macOS has no generic HID PID (force feedback) driver — unlike Windows DirectInput
and Linux `hid-pidff`, it only drives rumble on controllers it explicitly supports
(DualShock 4/DualSense, Xbox, MFi). Emulating a DS4 is therefore how this example
gets OS-native rumble on macOS. The DS4 rumble motors have independent magnitudes,
but the GameCube motor is on/off only, so any non-zero magnitude turns it on.

The GameCube controls are mapped onto the DS4 layout as follows:

| GameCube | DualShock 4        |
| -------- | ------------------ |
| A        | Cross              |
| B        | Circle             |
| X        | Square             |
| Y        | Triangle           |
| Start    | Options            |
| Z        | R1                 |
| L        | L2 (analog + click)|
| R        | R2 (analog + click)|
| D-pad    | D-pad              |
| Stick    | Left stick         |
| C-stick  | Right stick        |

The PS4 console authentication handshake is **not** implemented, so this adapter
works with macOS (and other PC hosts) but will not be accepted by a PS4/PS5.


## Building

```bash
cmake -Bbuild . && cmake --build build
```

## Flashing

Enter bootloader mode by holding the BOOTSEL button while plugging in the Pico. Then copy the generated `build/gcn-adapter-ds4.uf2` file to the RPI-RP2 drive that appears, or use `picotool`:

```bash
picotool load -f build/gcn-adapter-ds4.uf2
```

## Wiring

- GameCube controller 5V to the VSYS pin on your Pico
- GameCube controller 3.3V to the 3.3V pin on your Pico
- GameCube controller GND to a GND pin on your Pico
- Pick any GPIO for the data line, and connect it to the data line on the GameCube controller port

Pull-up resistor is required for the data line, the recommended value is 750Ω.