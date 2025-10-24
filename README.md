# libjoybus

An implementation of the Joybus protocol used by N64 and GameCube controllers,
for 32-bit microcontrollers.

## Supported Platforms

The following platforms are currently supported:

- Raspberry Pi RP2040 and RP2350 series MCUs
- Silicon Labs EFR32 Series 1 and Series 2 MCUs

## Communicating with Controllers

In *host mode*, `libjoybus` allows a microcontroller to communicate with N64 and
GameCube controllers. This allows you to use input data from N64 and GameCube
controllers in your projects.

```c
#include <joybus/joybus.h>
#include <joybus/host/gamecube.h>

struct joybus_rp2xxx rp2xxx_bus;
struct joybus *bus = JOYBUS(&rp2xxx_bus);

struct joybus_gc_controller_input input;
uint8_t joybus_buffer[JOYBUS_BLOCK_SIZE];

void poll_cb(struct joybus *bus, int result, void *user_data) {
  // Check for errors
  // TODO

  // Unpack the input data from the response
  joybus_gcn_unpack_input(&input, joybus_buffer, JOYBUS_GCN_ANALOG_MODE_3);

  // Do something with the input data
  if(input.buttons & JOYBUS_GCN_BUTTON_A) {
    // The A button is pressed
  }
}

void main() {
  // Initialize the Joybus
  joybus_rp2xxx_init(&rp2xxx_bus, MY_GPIO, pio0);
  joybus_enable(bus);

  while (1) {
    // Read a GameCube controller in analog mode 3 with the rumble motor off
    joybus_gcn_read(bus, JOYBUS_GCN_ANALOG_MODE_3, JOYBUS_GCN_MOTOR_STOP, joybus_buffer,
                    poll_cb, NULL);

    sleep_ms(10);
  }
}
```

## Emulating a Controller

In *target mode*, `libjoybus` allows a microcontroller to act as an N64 or GameCube
controller. This allows you to create custom controllers that can interface with
N64, GameCube, and Wii consoles.

```c
#include <joybus/joybus.h>
#include <joybus/target/gc_controller.h>

struct joybus_rp2xxx rp2xxx_bus;
struct joybus *bus = JOYBUS(&rp2xxx_bus);
struct joybus_gc_controller controller;

void main() {
  // Initialize the Joybus
  joybus_rp2xxx_init(&rp2xxx_bus, MY_GPIO, pio0);
  joybus_enable(bus);

  // Initialize a GameCube controller target
  joybus_gc_controller_init(&controller, JOYBUS_GAMECUBE_CONTROLLER);

  // Register the target on the bus
  joybus_target_register(bus, JOYBUS_TARGET(&controller));

  // At this point the target will respond to commands from a connected console!
  // Modify the input state as needed, for example based on GPIO or ADC readings
  while (1) {
    // Clear previous button state
    controller.input.buttons &= ~JOYBUS_GCN_BUTTON_MASK;

    // Simulate pressing the A button
    controller.input.buttons |= JOYBUS_GCN_BUTTON_A;

    // Simulate setting the analog stick position
    controller.input.stick_x = 200;
    controller.input.stick_y = 200;

    sleep_ms(10);
  }
}
```
