# Pico SDK N64 Controller Target Example

Act as a N64 controller on Pi Pico device using the Pico SDK. Maps button presses from GPIO inputs and stick positions from ADC channels to the N64 controller state.

## Building

```bash
cmake -Bbuild . && cmake --build build
```

## Flashing

Enter bootloader mode by holding the BOOTSEL button while plugging in the Pico. Then copy the generated `build/n64-target.uf2` file to the RPI-RP2 drive that appears, or use `picotool`:

```bash
picotool load -f build/n64-target.uf2
```
