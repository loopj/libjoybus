# N64 USB HID Adapter Example

Implements a simple single port N64 controller to USB-HID gamepad adapter.


## Building

```bash
cmake -Bbuild . && cmake --build build
```

## Flashing

Enter bootloader mode by holding the BOOTSEL button while plugging in the Pico. Then copy the generated `build/n64-adapter-usb-hid.uf2` file to the RPI-RP2 drive that appears, or use `picotool`:

```bash
picotool load -f build/n64-adapter-usb-hid.uf2
```

## Wiring

- N64 controller 3.3V to the 3.3V pin on your Pico
- N64 controller GND to a GND pin on your Pico
- Pick any GPIO for the data line, and connect it to the data line on the N64 controller port

Pull-up resistor is required for the data line, the recommended value is 750Ω.