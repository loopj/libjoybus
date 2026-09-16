#include "pico/rand.h"
#include "pico/stdlib.h"

#include <joybus/joybus.h>
#include <joybus/backend/rp2xxx.h>

// GPIO for the Joybus data line
#define JOYBUS_GPIO 0

// Joybus and target instances
static struct joybus_rp2xxx rp2xxx_bus;
static struct joybus *bus = JOYBUS(&rp2xxx_bus);
static struct joybus_target_n64_controller n64_controller;
static struct joybus_target_n64_pak_controller controller_pak;

// A single bank of Controller Pak storage, held in RAM
static uint8_t pak_memory[JOYBUS_N64_PAK_BANK_SIZE];

int main()
{
  // Initialize the Joybus
  joybus_rp2xxx_init(&rp2xxx_bus, joybus_rp2xxx_config_default(JOYBUS_GPIO));

  // Initialize a N64 controller target as a standard controller
  joybus_target_n64_controller_init(&n64_controller);

  // Format the pak, since RAM starts empty on every boot
  joybus_n64_pak_fs_format(pak_memory, 1, get_rand_32());

  // Initialize a single bank controller pak over that memory and plug it into the controller
  joybus_target_n64_pak_controller_init(&controller_pak, 1);
  joybus_target_n64_pak_controller_set_memory(&controller_pak, pak_memory);
  joybus_target_n64_controller_attach_pak(&n64_controller, JOYBUS_TARGET_N64_PAK(&controller_pak));

  // Attach the target to the bus
  joybus_attach_target(bus, JOYBUS_TARGET(&n64_controller));

  // Enable the Joybus in target mode
  joybus_enable(bus, JOYBUS_MODE_TARGET);

  while (1) {
    // Read your inputs into n64_controller.input here

    // Chill for a bit
    sleep_ms(10);
  }

  return 0;
}
