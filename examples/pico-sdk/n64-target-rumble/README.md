# Pico SDK N64 Rumble Pak Target Example

Act as a N64 controller with a Rumble Pak on Pi Pico device using the Pico SDK. The controller has no inputs wired up, see the `n64-target` example for those.

The motor GPIO is driven while the console runs the motor.

## Building

```bash
cmake -Bbuild . && cmake --build build
```

## Flashing

Enter bootloader mode by holding the BOOTSEL button while plugging in the Pico. Then copy the generated `build/n64-target-rumble.uf2` file to the RPI-RP2 drive that appears, or use `picotool`:

```bash
picotool load -f build/n64-target-rumble.uf2
```
