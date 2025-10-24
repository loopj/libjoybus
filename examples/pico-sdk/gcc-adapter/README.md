# GameCube Controller Adapter for Raspberry Pi Pico

A full implementation of a 4-port USB GameCube Controller Adapter, compatible with the official Wii U GameCube Controller Adapter protocols.

By default it polls the Joybus at 1kHz and tells USB hosts to poll at 1kHz as well, which results in lower input latency compared to the official adapter.

- Supports up to 4 controllers
- Supports rumble
- Works on Wii U, Switch, Switch 2, and Dolphin

## Configuration

The adapter can be configured by modifying the values in `config.h`:

## Building

```bash
cmake -Bbuild . && cmake --build build
```

## Flashing

Enter bootloader mode by holding the BOOTSEL button while plugging in the Pico. Then copy the generated `build/gcc-adapter.uf2` file to the RPI-RP2 drive that appears, or use `picotool`:

```bash
picotool load -f build/gcc-adapter.uf2
```

## Wiring

- GameCube controller 5V to the VSYS pin on your Pico
- GameCube controller 3.3V to the 3.3V pin on your Pico
- GameCube controller GND to a GND pin on your Pico
- Pick any 4 GPIO pins for the data lines, and connect them to the data lines on the GameCube controller ports

Pull-up resistors are required on each data line, the recommended value is 750Ω.

## GCC Adapter HID Tool

The GameCube Controller Adapter requires a "start polling" HID report to be sent before it will start sending controller data, a Python script is provided which can send the required HID reports. If you are using this adapter with a Wii U, Switch, Switch 2, or Dolphin, you do not need to use this tool as those systems will send the required reports automatically.