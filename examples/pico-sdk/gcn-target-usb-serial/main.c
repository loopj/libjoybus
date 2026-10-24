#include <stdio.h>
#include <string.h>

#include "hardware/sync.h"
#include "pico/stdlib.h"

#include <joybus/joybus.h>
#include <joybus/backend/rp2xxx.h>

// GPIO for the Joybus data line
#define JOYBUS_GPIO    0

// Return to neutral once no valid line has arrived for this long
#define FAILSAFE_MS    250

// Characters in a state line, "BBBB SX SY CX CY LT RT"
#define STATE_LINE_LEN 22

// Joybus and target instances
static struct joybus_rp2xxx rp2xxx_bus;
static struct joybus *bus = JOYBUS(&rp2xxx_bus);
static struct joybus_target_gcn_controller gcn_controller;

// Characters of the state line being received
static char line[STATE_LINE_LEN + 1];

// Copy a state into the controller input, keeping the status bits the library owns
static void set_input(const struct joybus_gcn_controller_state *state)
{
  struct joybus_gcn_controller_state *input = &gcn_controller.input;

  // Block the Joybus interrupt so a poll never sees a half-updated state
  uint32_t interrupts = save_and_disable_interrupts();

  // Replace the buttons and the six analog bytes that follow them
  input->buttons = (input->buttons & ~JOYBUS_GCN_BUTTON_MASK) | (state->buttons & JOYBUS_GCN_BUTTON_MASK);
  memcpy(&input->stick_x, &state->stick_x, 6);

  // Let the Joybus interrupt run again
  restore_interrupts(interrupts);
}

int main()
{
  // Initialize stdio over USB serial
  stdio_init_all();

  // Initialize the Joybus
  joybus_rp2xxx_init(&rp2xxx_bus, joybus_rp2xxx_config_default(JOYBUS_GPIO));

  // Initialize a GameCube controller target as a standard controller
  joybus_target_gcn_controller_init(&gcn_controller);

  // Attach the target to the bus
  joybus_attach_target(bus, JOYBUS_TARGET(&gcn_controller));

  // Enable the Joybus in target mode
  joybus_enable(bus, JOYBUS_MODE_TARGET);

  // Start with an empty line and the failsafe disarmed
  size_t line_len               = 0;
  absolute_time_t failsafe_time = at_the_end_of_time;

  while (true) {
    // Read the next character from the computer, if one has arrived
    int ch = getchar_timeout_us(0);

    if (ch >= 0 && ch != '\r' && ch != '\n') {
      // Collect the character, dropping any past the end of the buffer
      if (line_len < STATE_LINE_LEN)
        line[line_len++] = ch;
    } else if (ch >= 0 && line_len > 0) {
      // Parse the finished line into a state
      struct joybus_gcn_controller_state state;
      unsigned short buttons;
      line[line_len] = '\0';
      line_len       = 0;
      int fields = sscanf(line, "%4hx %2hhx %2hhx %2hhx %2hhx %2hhx %2hhx", &buttons, &state.stick_x, &state.stick_y,
                          &state.substick_x, &state.substick_y, &state.trigger_left, &state.trigger_right);

      // Apply a complete state and rearm the failsafe, ignoring a malformed line
      if (fields == 7) {
        state.buttons = buttons;
        set_input(&state);
        failsafe_time = make_timeout_time_ms(FAILSAFE_MS);
      }
    }

    // Return to neutral once the computer stops sending
    if (time_reached(failsafe_time)) {
      set_input(&gcn_controller.origin);
      failsafe_time = at_the_end_of_time;
    }
  }

  return 0;
}
