#include <joybus/commands.h>
#include <joybus/host/common.h>

int joybus_identify(struct joybus *bus, uint8_t *response, joybus_transfer_cb_t callback, void *user_data)
{
  bus->command_buffer[0] = JOYBUS_CMD_IDENTIFY;

  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_IDENTIFY_TX, response, JOYBUS_CMD_IDENTIFY_RX, callback,
                         user_data);
}

int joybus_reset(struct joybus *bus, uint8_t *response, joybus_transfer_cb_t callback, void *user_data)
{
  bus->command_buffer[0] = JOYBUS_CMD_RESET;

  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_RESET_TX, response, JOYBUS_CMD_RESET_RX, callback,
                         user_data);
}