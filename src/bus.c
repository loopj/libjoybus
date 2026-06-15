#include <joybus/bus.h>

void joybus_sync_cb(struct joybus *bus, int status, void *user_data)
{
  struct joybus_sync_ctx *ctx = (struct joybus_sync_ctx *)user_data;
  ctx->status                 = status;
  ctx->done                   = true;
}

int joybus_sync(int start_status, struct joybus_sync_ctx *ctx)
{
  // Propagate a failure to start the operation
  if (start_status < 0)
    return start_status;

  // Wait for the completion callback to record the status
  while (!ctx->done) {
    // Busy-wait
  }

  return ctx->status;
}

int joybus_transfer_sync(struct joybus *bus, const uint8_t *write_buf, uint8_t write_len, uint8_t *read_buf,
                         uint8_t read_len)
{
  struct joybus_sync_ctx ctx = {0};
  int status = joybus_transfer(bus, write_buf, write_len, read_buf, read_len, joybus_sync_cb, &ctx);
  return joybus_sync(status, &ctx);
}
