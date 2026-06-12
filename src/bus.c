#include <joybus/bus.h>

// Synchronous transfer context
struct joybus_sync_ctx {
  volatile bool done;
  volatile int result;
};

// Callback for synchronous transfer completion
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
