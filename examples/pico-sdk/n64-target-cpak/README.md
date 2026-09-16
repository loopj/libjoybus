# Pico SDK N64 Controller Pak Target Example

Act as a N64 controller with a Controller Pak on Pi Pico device using the Pico SDK. The controller has no inputs wired up, see the `n64-target` example for those.

The Controller Pak is a single bank held in RAM. It is formatted at boot, so a console sees an empty pak, and its contents are lost on reset.

## Building

```bash
cmake -Bbuild . && cmake --build build
```

## Flashing

Enter bootloader mode by holding the BOOTSEL button while plugging in the Pico. Then copy the generated `build/n64-target-cpak.uf2` file to the RPI-RP2 drive that appears, or use `picotool`:

```bash
picotool load -f build/n64-target-cpak.uf2
```
