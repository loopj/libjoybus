# Pico SDK N64 Host Example

Act as an N64 Joybus host on RP2XXX devices using the Pico SDK.

- Polls for a controller until one is connected
- Checks for a rumble pak once a controller is connected, and then periodically
- Enables a GPIO when the A button is pressed
- Enables the rumble motor while the A and B buttons are pressed

This example demonstrates the synchronous host-side APIs, which are easier to use but busy-wait for transfers to complete.

## Building

```bash
cmake -Bbuild . && cmake --build build
```

## Flashing

Enter bootloader mode by holding the BOOTSEL button while plugging in the Pico. Then copy the generated `build/n64-host.uf2` file to the RPI-RP2 drive that appears, or use `picotool`:

```bash
picotool load -f build/n64-host.uf2
```
