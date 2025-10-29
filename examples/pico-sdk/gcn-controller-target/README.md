# Pico SDK GameCube Controller Target Example

Act as a GameCube controller on RP2XXX devices using the Pico SDK. Simulates A button presses when a physical button is pressed on the Pico.

## Building

```bash
cmake -Bbuild . && cmake --build build
```

## Flashing

Enter bootloader mode by holding the BOOTSEL button while plugging in the Pico. Then copy the generated `build/app.uf2` file to the RPI-RP2 drive that appears, or use `picotool`:

```bash
picotool load -f build/app.uf2
```
