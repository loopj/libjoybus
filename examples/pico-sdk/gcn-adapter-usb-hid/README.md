# Simple GameCube USB Adapter Example

Implements a simple single port GameCube controller to USB-HID gamepad adapter.


## Building

```bash
cmake -Bbuild . && cmake --build build
```

## Flashing

Enter bootloader mode by holding the BOOTSEL button while plugging in the Pico. Then copy the generated `build/gcn-adapter-usb-hid.uf2` file to the RPI-RP2 drive that appears, or use `picotool`:

```bash
picotool load -f build/gcn-adapter-usb-hid.uf2
```

## Wiring

- GameCube controller 5V to the VSYS pin on your Pico
- GameCube controller 3.3V to the 3.3V pin on your Pico
- GameCube controller GND to a GND pin on your Pico
- Pick any GPIO for the data line, and connect it to the data line on the GameCube controller port

Pull-up resistor is required for the data line, the recommended value is 750Ω.