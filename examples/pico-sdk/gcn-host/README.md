# Pico SDK GameCube Host Example

Act as an Joybus host on RP2XXX devices using the Pico SDK. Toggle a GPIO when the A button is pressed on a connected GameCube controller.

## Building

```bash
cmake -Bbuild . && cmake --build build
```

## Flashing

Enter bootloader mode by holding the BOOTSEL button while plugging in the Pico. Then copy the generated `build/app.uf2` file to the RPI-RP2 drive that appears, or use `picotool`:

```bash
picotool load -f build/app.uf2
```