#include <stdio.h>
#include "pico/stdlib.h"

#include <joybus/joybus.h>
#include <joybus/backend/rp2xxx.h>

#define SI_DATA_GPIO  12

struct joybus_rp2xxx rp2xxx_bus;
struct joybus *bus = JOYBUS(&rp2xxx_bus);

static struct joybus_gc_controller gc_controller;

int main()
{
  stdio_init_all(); // USB serial
  sleep_ms(2000);   // allow time to connect

  // Initialize Joybus
  joybus_rp2xxx_init(&rp2xxx_bus, SI_DATA_GPIO, pio0);
  joybus_enable(bus);

  // Initialize controller target
  joybus_gc_controller_init(&gc_controller, JOYBUS_GAMECUBE_CONTROLLER);

  // Register the target on the bus
  joybus_target_register(bus, JOYBUS_TARGET(&gc_controller));

  while (1) {
    // Clear previous button state
    gc_controller.input.buttons &= ~JOYBUS_GCN_BUTTON_MASK;

    // Read USB serial input (non-blocking)
    int ch = getchar_timeout_us(0);

    if (ch != PICO_ERROR_TIMEOUT) {
      if (ch == 'a')
        gc_controller.input.buttons |= JOYBUS_GCN_BUTTON_A;
      if (ch == 'b')
        gc_controller.input.buttons |= JOYBUS_GCN_BUTTON_B;
      if (ch == 'x')
        gc_controller.input.buttons |= JOYBUS_GCN_BUTTON_X;
      if (ch == 'y')
        gc_controller.input.buttons |= JOYBUS_GCN_BUTTON_Y;
      if (ch == 's')
        gc_controller.input.buttons |= JOYBUS_GCN_BUTTON_START;
      if (ch == 'z')
        gc_controller.input.buttons |= JOYBUS_GCN_BUTTON_Z;
      if (ch == 'l')
        gc_controller.input.buttons |= JOYBUS_GCN_BUTTON_L;
      if (ch == 'r')
        gc_controller.input.buttons |= JOYBUS_GCN_BUTTON_R;
      if (ch == 'u')
        gc_controller.input.buttons |= JOYBUS_GCN_BUTTON_UP;
      if (ch == 'd')
        gc_controller.input.buttons |= JOYBUS_GCN_BUTTON_DOWN;
      if (ch == 'L')
        gc_controller.input.buttons |= JOYBUS_GCN_BUTTON_LEFT;
      if (ch == 'R')
        gc_controller.input.buttons |= JOYBUS_GCN_BUTTON_RIGHT;
    }

    sleep_ms(10);
  }

  return 0;
}
