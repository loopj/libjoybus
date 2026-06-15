#include <string.h>

#include <joybus/bus.h>
#include <joybus/checksum.h>
#include <joybus/errors.h>
#include <joybus/host/n64.h>
#include <joybus/host/n64_rumble_pak.h>

// Probe address and values
#define RUMBLE_PAK_PROBE_ADDR     0x8000
#define RUMBLE_PAK_SIGNATURE      0x80
#define RUMBLE_PAK_ANTI_SIGNATURE 0xFE
#define RUMBLE_PAK_PROBE_BYTE     (JOYBUS_PAK_BLOCK_SIZE - 1)

// Motor address and values
#define RUMBLE_PAK_MOTOR_ADDR     0xC000
#define RUMBLE_PAK_MOTOR_ON       0x01
#define RUMBLE_PAK_MOTOR_OFF      0x00

// Probe steps
enum {
  PROBE_WROTE_ANTI_SIGNATURE,
  PROBE_READ_ANTI_SIGNATURE,
  PROBE_WROTE_SIGNATURE,
  PROBE_READ_SIGNATURE,
};

static void probe_cb(struct joybus *bus, int result, void *user_data);

// Fire the saved user callback with a result
static void probe_finish(struct joybus *bus, int result)
{
  if (bus->host_op.callback)
    bus->host_op.callback(bus, result, bus->host_op.user_data);
}

// Write a uniform block to the probe register and continue the chain
static int probe_write(struct joybus *bus, uint8_t value)
{
  uint8_t block[JOYBUS_PAK_BLOCK_SIZE];
  memset(block, value, sizeof(block));

  return joybus_n64_pak_write_async(bus, RUMBLE_PAK_PROBE_ADDR, block, bus->response_buffer, probe_cb, NULL);
}

// Read the probe register back and continue the chain
static int probe_read(struct joybus *bus)
{
  return joybus_n64_pak_read_async(bus, RUMBLE_PAK_PROBE_ADDR, bus->response_buffer, probe_cb, NULL);
}

// Advance the probe one step each time a transfer completes
static void probe_cb(struct joybus *bus, int result, void *user_data)
{
  // Bail out of the chain on any transfer error
  if (result < 0) {
    probe_finish(bus, result);
    return;
  }

  switch (bus->host_op.arg) {
    case PROBE_WROTE_ANTI_SIGNATURE:
      // Non-signature written, read it back
      bus->host_op.arg = PROBE_READ_ANTI_SIGNATURE;
      result           = probe_read(bus);
      break;

    case PROBE_READ_ANTI_SIGNATURE:
      // A controller pak reads the non-signature back, a rumble pak does not
      if (bus->response_buffer[RUMBLE_PAK_PROBE_BYTE] == RUMBLE_PAK_ANTI_SIGNATURE) {
        probe_finish(bus, -JOYBUS_ERR_NO_DEVICE);
        return;
      }
      bus->host_op.arg = PROBE_WROTE_SIGNATURE;
      result           = probe_write(bus, RUMBLE_PAK_SIGNATURE);
      break;

    case PROBE_WROTE_SIGNATURE:
      // Signature written, read it back
      bus->host_op.arg = PROBE_READ_SIGNATURE;
      result           = probe_read(bus);
      break;

    case PROBE_READ_SIGNATURE:
      // A rumble pak reads the signature back once enabled
      if (bus->response_buffer[RUMBLE_PAK_PROBE_BYTE] != RUMBLE_PAK_SIGNATURE)
        result = -JOYBUS_ERR_NO_DEVICE;

      probe_finish(bus, result >= 0 ? 0 : result);
      return;
  }

  // A transfer that failed to start ends the chain
  if (result < 0)
    probe_finish(bus, result);
}

static void motor_write_cb(struct joybus *bus, int result, void *user_data)
{
  // Check for CRC errors
  if (result >= 0 && bus->host_op.arg != bus->response_buffer[0])
    result = -JOYBUS_ERR_CHECKSUM;

  // Fire the user callback
  if (bus->host_op.callback)
    bus->host_op.callback(bus, result, bus->host_op.user_data);
}

static int motor_write(struct joybus *bus, uint8_t value, joybus_transfer_cb callback, void *user_data) {
  // Fill a block with bytes
  uint8_t block[JOYBUS_PAK_BLOCK_SIZE];
  memset(block, value, sizeof(block));

  // Save the callback, user data, and expected checksum for later
  bus->host_op.callback  = callback;
  bus->host_op.user_data = user_data;
  bus->host_op.arg       = joybus_data_checksum(block, sizeof(block));

  return joybus_n64_pak_write_async(bus, RUMBLE_PAK_MOTOR_ADDR, block, bus->response_buffer, motor_write_cb, NULL);
}

int joybus_n64_rumble_pak_init(struct joybus *bus)
{
  struct joybus_sync_ctx ctx = {0};
  return joybus_sync(joybus_n64_rumble_pak_init_async(bus, joybus_sync_cb, &ctx), &ctx);
}

int joybus_n64_rumble_pak_init_async(struct joybus *bus, joybus_transfer_cb callback, void *user_data)
{
  // Save the user callback for the end of the probe chain
  bus->host_op.callback  = callback;
  bus->host_op.user_data = user_data;

  // Save the initial probe state and start the chain
  bus->host_op.arg       = PROBE_WROTE_ANTI_SIGNATURE;
  return probe_write(bus, RUMBLE_PAK_ANTI_SIGNATURE);
}

int joybus_n64_rumble_pak_start(struct joybus *bus)
{
  struct joybus_sync_ctx ctx = {0};
  return joybus_sync(joybus_n64_rumble_pak_start_async(bus, joybus_sync_cb, &ctx), &ctx);
}

int joybus_n64_rumble_pak_start_async(struct joybus *bus, joybus_transfer_cb callback, void *user_data)
{
  return motor_write(bus, RUMBLE_PAK_MOTOR_ON, callback, user_data);
}

int joybus_n64_rumble_pak_stop(struct joybus *bus)
{
  struct joybus_sync_ctx ctx = {0};
  return joybus_sync(joybus_n64_rumble_pak_stop_async(bus, joybus_sync_cb, &ctx), &ctx);
}

int joybus_n64_rumble_pak_stop_async(struct joybus *bus, joybus_transfer_cb callback, void *user_data)
{
  return motor_write(bus, RUMBLE_PAK_MOTOR_OFF, callback, user_data);
}
