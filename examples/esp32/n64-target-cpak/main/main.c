#include <esp_random.h>
#include <freertos/FreeRTOS.h>

#include <joybus/joybus.h>
#include <joybus/backend/esp32.h>

// GPIO for the Joybus data line
#define JOYBUS_GPIO 1

// Joybus and target instances.
static struct joybus_esp32 esp32_bus;
static struct joybus *bus = JOYBUS(&esp32_bus);
static struct joybus_target_n64_controller n64_controller;
static struct joybus_target_n64_pak_controller controller_pak;

// A single bank of Controller Pak storage, held in RAM
static uint8_t pak_memory[JOYBUS_N64_PAK_BANK_SIZE];

void app_main(void)
{
  // Initialize the Joybus.
  joybus_esp32_init(&esp32_bus, joybus_esp32_config_default(JOYBUS_GPIO));

  // Initialize a N64 controller target as a standard controller.
  joybus_target_n64_controller_init(&n64_controller);

  // Format the pak, since RAM starts empty on every boot.
  joybus_n64_pak_fs_format(pak_memory, 1, esp_random());

  // Initialize a single bank controller pak over that memory and plug it into the controller.
  joybus_target_n64_pak_controller_init(&controller_pak, 1);
  joybus_target_n64_pak_controller_set_memory(&controller_pak, pak_memory);
  joybus_target_n64_controller_attach_pak(&n64_controller, JOYBUS_TARGET_N64_PAK(&controller_pak));

  // Attach the target to the bus.
  joybus_attach_target(bus, JOYBUS_TARGET(&n64_controller));

  // Enable the Joybus in target mode.
  joybus_enable(bus, JOYBUS_MODE_TARGET);

  while (1) {
    // Read your inputs into n64_controller.input here

    // Chill for a bit
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
