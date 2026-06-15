#include <joybus/commands.h>
#include <joybus/host/common.h>

int joybus_identify(struct joybus *bus, struct joybus_id *response)
{
  struct joybus_sync_ctx ctx = {0};
  return joybus_sync(joybus_identify_async(bus, response, joybus_sync_cb, &ctx), &ctx);
}

int joybus_identify_async(struct joybus *bus, struct joybus_id *response, joybus_transfer_cb_t callback,
                          void *user_data)
{
  bus->command_buffer[0] = JOYBUS_CMD_IDENTIFY;

  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_IDENTIFY_TX, (uint8_t *)response, JOYBUS_CMD_IDENTIFY_RX,
                         callback, user_data);
}

int joybus_reset(struct joybus *bus, struct joybus_id *response)
{
  struct joybus_sync_ctx ctx = {0};
  return joybus_sync(joybus_reset_async(bus, response, joybus_sync_cb, &ctx), &ctx);
}

int joybus_reset_async(struct joybus *bus, struct joybus_id *response, joybus_transfer_cb_t callback, void *user_data)
{
  bus->command_buffer[0] = JOYBUS_CMD_RESET;

  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_RESET_TX, (uint8_t *)response, JOYBUS_CMD_RESET_RX,
                         callback, user_data);
}
