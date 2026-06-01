#include <joybus/commands.h>
#include <joybus/host/common.h>

struct joybus_sync_ctx {
  volatile bool done;
  volatile int result;
};

static void joybus_sync_cb(struct joybus *bus, int result, void *user_data)
{
  struct joybus_sync_ctx *ctx = (struct joybus_sync_ctx *)user_data;
  ctx->result                 = result;
  ctx->done                   = true;
}

int joybus_transfer_sync(struct joybus *bus, const uint8_t *write_buf, uint8_t write_len, uint8_t *read_buf,
                         uint8_t read_len)
{
  struct joybus_sync_ctx ctx = {0};

  int result;
  result = joybus_transfer(bus, write_buf, write_len, read_buf, read_len, joybus_sync_cb, &ctx);
  if (result < 0)
    return result;

  while (!ctx.done) {
    // Busy-wait
  }

  return ctx.result;
}

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