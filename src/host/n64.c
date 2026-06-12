/**
 * N64 host functions.
 */

#include <string.h>

#include <joybus/bus.h>
#include <joybus/checksum.h>
#include <joybus/commands.h>
#include <joybus/common/n64_controller.h>
#include <joybus/host/n64.h>

int joybus_n64_read(struct joybus *bus, struct joybus_n64_controller_state *response)
{
  // Build the command
  bus->command_buffer[0] = JOYBUS_CMD_N64_READ;

  // Transfer the command and read the response directly into the response buffer
  return joybus_transfer_sync(bus, bus->command_buffer, JOYBUS_CMD_N64_READ_TX, (uint8_t *)response,
                              JOYBUS_CMD_N64_READ_RX);
}

int joybus_n64_read_async(struct joybus *bus, struct joybus_n64_controller_state *response,
                          joybus_transfer_cb_t callback, void *user_data)
{
  // Build the command
  bus->command_buffer[0] = JOYBUS_CMD_N64_READ;

  // Transfer the command and read the response directly into the response buffer
  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_N64_READ_TX, (uint8_t *)response, JOYBUS_CMD_N64_READ_RX,
                         callback, user_data);
}

int joybus_n64_pak_write_async(struct joybus *bus, uint16_t addr, const uint8_t *data, uint8_t *response,
                               joybus_transfer_cb_t callback, void *user_data)
{
  // Generate address with checksum
  uint16_t with_checksum = (addr & 0xFFE0) | joybus_address_checksum(addr >> 5);

  // Build command
  bus->command_buffer[0] = JOYBUS_CMD_N64_PAK_WRITE;
  bus->command_buffer[1] = (uint8_t)(with_checksum >> 8);
  bus->command_buffer[2] = (uint8_t)(with_checksum & 0xFF);

  // Copy data to be written
  memcpy(&bus->command_buffer[3], data, 32);

  // Send command
  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_N64_PAK_WRITE_TX, response, JOYBUS_CMD_N64_PAK_WRITE_RX,
                         callback, user_data);
}

int joybus_n64_pak_read_async(struct joybus *bus, uint16_t addr, uint8_t *response, joybus_transfer_cb_t callback,
                              void *user_data)
{
  // Generate address with checksum
  uint16_t with_checksum = (addr & 0xFFE0) | joybus_address_checksum(addr >> 5);

  // Build command
  bus->command_buffer[0] = JOYBUS_CMD_N64_PAK_READ;
  bus->command_buffer[1] = (uint8_t)(with_checksum >> 8);
  bus->command_buffer[2] = (uint8_t)(with_checksum & 0xFF);

  // Send command
  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_N64_PAK_READ_TX, response, JOYBUS_CMD_N64_PAK_READ_RX,
                         callback, user_data);
}
