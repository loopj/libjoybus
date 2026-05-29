# Interactive Joybus Shell Example

Pico SDK application which presents an interactive console for sending Joybus
commands to connected Joybus targets.

## Building

```bash
cmake -Bbuild . && cmake --build build
```

## Flashing

Enter bootloader mode by holding the BOOTSEL button while plugging in the Pico. Then copy the generated `build/joybus-shell.uf2` file to the RPI-RP2 drive that appears, or use `picotool`:

```bash
picotool load -f build/joybus-shell.uf2
```

## Usage

Connect to the Pico's USB serial port with a serial terminal, and type `help` to see a list of available commands.