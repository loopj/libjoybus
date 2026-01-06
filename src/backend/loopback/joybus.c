#include <string.h>

#include <joybus/bus.h>
#include <joybus/target.h>
#include <joybus/backend/loopback.h>

static uint8_t *response_buffer;
static size_t response_length;

static int joybus_loopback_enable(struct joybus *bus)
{
  return 0;
}

static int joybus_loopback_disable(struct joybus *bus)
{
  return 0;
}

static void handle_command_response(const uint8_t *buffer, uint8_t result, void *user_data)
{
  if (response_buffer == NULL || result <= 0)
    return;

  // Copy the bytes provided by the target into the read buffer
  memcpy(response_buffer, buffer, result);
  response_length = result;
}

static int joybus_loopback_transfer(struct joybus *bus, const uint8_t *write_buf, uint8_t write_len, uint8_t *read_buf,
                                    uint8_t read_len, joybus_transfer_cb_t callback, void *user_data)
{
  // Save the buffer to write the response to, default to zero length
  response_buffer = read_buf;
  response_length = 0;

  // Handle the command using the registered target
  for (int i = 1; i <= write_len; i++) {
    int rc = joybus_target_byte_received(bus->target, write_buf, i, handle_command_response, bus);
    if (rc == 0) {
      // No more bytes expected
      break;
    } else if (rc < 0) {
      // Error handling command, or command not supported
      callback(bus, 0, user_data);
      return rc;
    }
  }

  // Call the transfer complete callback
  callback(bus, response_length, user_data);

  return 0;
}

static int joybus_loopback_target_register(struct joybus *bus, struct joybus_target *target)
{
  bus->target = target;
  return 0;
}

static int joybus_loopback_target_unregister(struct joybus *bus, struct joybus_target *target)
{
  bus->target = NULL;
  return 0;
}

static const struct joybus_api loopback_api = {
  .enable            = joybus_loopback_enable,
  .disable           = joybus_loopback_disable,
  .transfer          = joybus_loopback_transfer,
  .target_unregister = joybus_loopback_target_unregister,
  .target_register   = joybus_loopback_target_register,
};

int joybus_loopback_init(struct joybus *bus)
{
  bus->api    = &loopback_api;
  bus->target = NULL;

  return 0;
}