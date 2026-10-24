# Pico SDK GameCube Controller Target USB Serial Example

Act as a GameCube controller on RP2XXX devices using the Pico SDK. A computer drives the controller by sending its state over USB serial.

## Building

```bash
cmake -Bbuild . && cmake --build build
```

## Flashing

Enter bootloader mode by holding the BOOTSEL button while plugging in the Pico. Then copy the generated `build/gcn-target-usb-serial.uf2` file to the RPI-RP2 drive that appears, or use `picotool`:

```bash
picotool load -f build/gcn-target-usb-serial.uf2
```

## Protocol

Send one line per controller state. Each line holds the button bits, main stick X and Y, C-stick X and Y, then the L and R trigger values, all in hex.

```
BBBB SX SY CX CY LT RT
0000 80 80 80 80 00 00    Neutral
0001 ff 80 80 80 00 00    A held with the main stick full right
```

| Bit    | Button | Bit    | Button      |
| ------ | ------ | ------ | ----------- |
| `0001` | A      | `0100` | D-pad left  |
| `0002` | B      | `0200` | D-pad right |
| `0004` | X      | `0400` | D-pad down  |
| `0008` | Y      | `0800` | D-pad up    |
| `0010` | Start  | `1000` | Z           |
|        |        | `2000` | R           |
|        |        | `4000` | L           |

The controller returns to neutral when no valid line arrives for 250 ms, so resend the current state every frame.
