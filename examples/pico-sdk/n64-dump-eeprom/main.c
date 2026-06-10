#include <stdio.h>

#include "hardware/pwm.h"
#include "joybus/bus.h"
#include "joybus/identify.h"
#include "pico/stdio_usb.h"

#include <joybus/joybus.h>
#include <pico/time.h>
#include <joybus/backend/rp2xxx.h>

// GPIO definitions
#define JOYBUS_DATA_GPIO  12
#define JOYBUS_CLOCK_GPIO 13

// Joybus instance
static struct joybus_rp2xxx rp2xxx_bus;
static struct joybus *bus = JOYBUS(&rp2xxx_bus);

// Buffer for Joybus responses
static uint8_t joybus_response[JOYBUS_BLOCK_SIZE];

// Buffer for EEPROM contents, enough to hold 256 8-byte blocks (16K EEPROM)
static uint8_t eeprom_contents[2048];

// Generate a 1.953125MHz clock signal to clock the cartridge
static void enable_joybus_clock(void)
{
  gpio_set_function(JOYBUS_CLOCK_GPIO, GPIO_FUNC_PWM);
  uint slice     = pwm_gpio_to_slice_num(JOYBUS_CLOCK_GPIO);
  uint chan      = pwm_gpio_to_channel(JOYBUS_CLOCK_GPIO);
  pwm_config cfg = pwm_get_default_config();
  pwm_config_set_clkdiv_int(&cfg, 1);
  pwm_config_set_wrap(&cfg, 64 - 1);
  pwm_init(slice, &cfg, true);
  pwm_set_chan_level(slice, chan, 32);
}

// Read the entire EEPROM contents into the buffer
static void read_eeprom(uint8_t *buf, size_t block_count)
{
  for (size_t i = 0; i < block_count; i++)
  {
    bus->command_buffer[0] = 0x04;
    bus->command_buffer[1] = i;
    joybus_transfer_sync(bus, bus->command_buffer, 2, &buf[i * JOYBUS_N64_EEPROM_BLOCK_SIZE], JOYBUS_N64_EEPROM_BLOCK_SIZE);
    sleep_us(58);
  }
}

static void dump_eeprom(uint8_t block_count)
{
  // Read the EEPROM contents into the buffer
  read_eeprom(eeprom_contents, block_count);

  // Print the contents in hex editor style, 16 bytes per row
  uint16_t size = block_count * JOYBUS_N64_EEPROM_BLOCK_SIZE;
  for (uint16_t addr = 0; addr < size; addr += 16)
  {
    printf("%04X:", addr);
    for (uint8_t i = 0; i < 16; i++)
    {
      if (i % 8 == 0)
        printf(" ");
      printf(" %02X", eeprom_contents[addr + i]);
    }
    printf("\n");
  }
}

int main()
{
  // Initialize stdio for printf output
  stdio_init_all();

  // Wait for the USB serial connection before doing anything
  while (!stdio_usb_connected())
    sleep_ms(100);

  // Initialize the Joybus
  joybus_rp2xxx_init(&rp2xxx_bus, JOYBUS_DATA_GPIO, pio0);
  joybus_enable(bus);

  // Enable the external Joybus clock
  enable_joybus_clock();

  // Send the identify command and read the response
  bus->command_buffer[0] = 0x00;
  joybus_transfer_sync(bus, bus->command_buffer, 1, joybus_response, 3);

  // Parse the response to determine the EEPROM type
  uint16_t type = joybus_id_get_type(joybus_response);
  bool is_4k    = (type == JOYBUS_TYPE_N64_EEPROM);
  bool is_16k   = (type == (JOYBUS_TYPE_N64_EEPROM | JOYBUS_TYPE_N64_EEPROM_16K));
  printf("Detected N64 cart with %dK EEPROM\n", is_16k ? 16 : 4);

  // Read each 8-byte EEPROM block into the contents buffer
  uint16_t block_count = is_16k ? 256 : 64;
  dump_eeprom(block_count);

  while (1)
    tight_loop_contents();

  return 0;
}
