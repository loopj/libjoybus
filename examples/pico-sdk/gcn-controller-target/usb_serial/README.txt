# Pico SDK GameCube Controller Target USB Serial Example

Act as a GameCube controller on RP2XXX devices using the Pico SDK using a serial interface. Simulates button presses when an input is sent over a usb serial connection.

## Building

```bash
cmake -B build -DPICO_ENABLE_STDIO_USB=1 -DPICO_ENABLE_STDIO_UART=0 -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350 . && cmake --build build
```  
or  
```bash
cmake -B build -DPICO_ENABLE_STDIO_USB=1 -DPICO_ENABLE_STDIO_UART=0 . && cmake --build build
```  

## Flashing

Enter bootloader mode by holding the BOOTSEL button while plugging in the Pico. Then copy the generated `build/usb_serial.uf2` file to the RPI-RP2 drive that appears, or use `picotool`:

```bash
picotool load -f build/usb_serial.uf2
```