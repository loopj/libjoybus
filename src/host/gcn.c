#include <string.h>

#include "joybus/bus.h"
#include <joybus/commands.h>
#include "joybus/common/gcn_controller.h"
#include <joybus/host/gcn.h>

/*
 * Unpack a "short" 8-byte input state into a "full" input state, depending on
 * the analog mode. See enum joybus_gcn_analog_mode for more details.
 */
static int unpack_input_state(struct joybus_gcn_controller_state *dest, const uint8_t *src,
                              enum joybus_gcn_analog_mode analog_mode)
{
  // Copy button and stick data
  memcpy(dest, src, 4);

  // Unpack the remaining analog input data
  switch (analog_mode) {
    default:
      dest->substick_x    = src[4];
      dest->substick_y    = src[5];
      dest->trigger_left  = (src[6] & 0xF0);
      dest->trigger_right = (src[6] & 0x0F) << 4;
      dest->analog_a      = (src[7] & 0xF0);
      dest->analog_b      = (src[7] & 0x0F) << 4;
      break;
    case JOYBUS_GCN_ANALOG_MODE_1:
      dest->substick_x    = (src[4] & 0xF0);
      dest->substick_y    = (src[4] & 0x0F) << 4;
      dest->trigger_left  = src[5];
      dest->trigger_right = src[6];
      dest->analog_a      = (src[7] & 0xF0);
      dest->analog_b      = (src[7] & 0x0F) << 4;
      break;
    case JOYBUS_GCN_ANALOG_MODE_2:
      dest->substick_x    = (src[4] & 0xF0);
      dest->substick_y    = (src[4] & 0x0F) << 4;
      dest->trigger_left  = (src[5] & 0xF0);
      dest->trigger_right = (src[5] & 0x0F) << 4;
      dest->analog_a      = src[6];
      dest->analog_b      = src[7];
      break;
    case JOYBUS_GCN_ANALOG_MODE_3:
      dest->substick_x    = src[4];
      dest->substick_y    = src[5];
      dest->trigger_left  = src[6];
      dest->trigger_right = src[7];
      break;
    case JOYBUS_GCN_ANALOG_MODE_4:
      dest->substick_x = src[4];
      dest->substick_y = src[5];
      dest->analog_a   = src[6];
      dest->analog_b   = src[7];
      break;
  }

  return 0;
}

static void gcn_read_cb(struct joybus *bus, int result, void *user_data)
{
  // Unpack the response
  if (result >= 0)
    unpack_input_state((struct joybus_gcn_controller_state *)bus->host_op.response, bus->response_buffer,
                       bus->host_op.arg);

  // Fire the user callback if one is set
  if (bus->host_op.callback)
    bus->host_op.callback(bus, result, bus->host_op.user_data);
}

int joybus_gcn_read(struct joybus *bus, enum joybus_gcn_analog_mode analog_mode,
                    enum joybus_gcn_motor_state motor_state, struct joybus_gcn_controller_state *response)
{
  struct joybus_sync_ctx ctx = {0};
  return joybus_sync(joybus_gcn_read_async(bus, analog_mode, motor_state, response, joybus_sync_cb, &ctx), &ctx);
}

int joybus_gcn_read_async(struct joybus *bus, enum joybus_gcn_analog_mode analog_mode,
                          enum joybus_gcn_motor_state motor_state, struct joybus_gcn_controller_state *response,
                          joybus_transfer_cb_t callback, void *user_data)
{
  // Build the command
  bus->command_buffer[0] = JOYBUS_CMD_GCN_READ;
  bus->command_buffer[1] = analog_mode;
  bus->command_buffer[2] = motor_state;

  // Set up the host operation
  bus->host_op.callback  = callback;
  bus->host_op.user_data = user_data;
  bus->host_op.response  = (uint8_t *)response;
  bus->host_op.arg       = analog_mode;

  // Transfer the command
  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_GCN_READ_TX, bus->response_buffer, JOYBUS_CMD_GCN_READ_RX,
                         gcn_read_cb, NULL);
}

int joybus_gcn_read_origin(struct joybus *bus, struct joybus_gcn_controller_state *response)
{
  struct joybus_sync_ctx ctx = {0};
  return joybus_sync(joybus_gcn_read_origin_async(bus, response, joybus_sync_cb, &ctx), &ctx);
}

int joybus_gcn_read_origin_async(struct joybus *bus, struct joybus_gcn_controller_state *response,
                                 joybus_transfer_cb_t callback, void *user_data)
{
  // Build the command
  bus->command_buffer[0] = JOYBUS_CMD_GCN_READ_ORIGIN;

  // Transfer the command and read the response directly into the response buffer
  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_GCN_READ_ORIGIN_TX, (uint8_t *)response,
                         JOYBUS_CMD_GCN_READ_ORIGIN_RX, callback, user_data);
}

int joybus_gcn_calibrate(struct joybus *bus, struct joybus_gcn_controller_state *response)
{
  struct joybus_sync_ctx ctx = {0};
  return joybus_sync(joybus_gcn_calibrate_async(bus, response, joybus_sync_cb, &ctx), &ctx);
}

int joybus_gcn_calibrate_async(struct joybus *bus, struct joybus_gcn_controller_state *response,
                               joybus_transfer_cb_t callback, void *user_data)
{
  // Build the command
  bus->command_buffer[0] = JOYBUS_CMD_GCN_CALIBRATE;
  bus->command_buffer[1] = 0;
  bus->command_buffer[2] = 0;

  // Transfer the command and read the response directly into the response buffer
  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_GCN_CALIBRATE_TX, (uint8_t *)response,
                         JOYBUS_CMD_GCN_CALIBRATE_RX, callback, user_data);
}

int joybus_gcn_read_long(struct joybus *bus, enum joybus_gcn_motor_state motor_state,
                         struct joybus_gcn_controller_state *response)
{
  struct joybus_sync_ctx ctx = {0};
  return joybus_sync(joybus_gcn_read_long_async(bus, motor_state, response, joybus_sync_cb, &ctx), &ctx);
}

int joybus_gcn_read_long_async(struct joybus *bus, enum joybus_gcn_motor_state motor_state,
                               struct joybus_gcn_controller_state *response, joybus_transfer_cb_t callback,
                               void *user_data)
{
  // Build the command
  bus->command_buffer[0] = JOYBUS_CMD_GCN_READ_LONG;
  bus->command_buffer[1] = 0; // Analog mode ignored for full precision reads
  bus->command_buffer[2] = motor_state;

  // Transfer the command and read the response directly into the response buffer
  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_GCN_READ_LONG_TX, (uint8_t *)response,
                         JOYBUS_CMD_GCN_READ_LONG_RX, callback, user_data);
}

int joybus_gcn_probe_device(struct joybus *bus, uint8_t response[JOYBUS_CMD_GCN_PROBE_DEVICE_RX])
{
  struct joybus_sync_ctx ctx = {0};
  return joybus_sync(joybus_gcn_probe_device_async(bus, response, joybus_sync_cb, &ctx), &ctx);
}

int joybus_gcn_probe_device_async(struct joybus *bus, uint8_t response[JOYBUS_CMD_GCN_PROBE_DEVICE_RX],
                                  joybus_transfer_cb_t callback, void *user_data)
{
  // Build the command
  // TODO: Mirror the behavior of real systems for the args
  bus->command_buffer[0] = JOYBUS_CMD_GCN_PROBE_DEVICE;
  bus->command_buffer[1] = 0;
  bus->command_buffer[2] = 0;

  // Transfer the command and read the response directly into the response buffer
  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_GCN_PROBE_DEVICE_TX, response,
                         JOYBUS_CMD_GCN_PROBE_DEVICE_RX, callback, user_data);
}

int joybus_gcn_fix_device(struct joybus *bus, uint16_t wireless_id, struct joybus_id *response)
{
  struct joybus_sync_ctx ctx = {0};
  return joybus_sync(joybus_gcn_fix_device_async(bus, wireless_id, response, joybus_sync_cb, &ctx), &ctx);
}

int joybus_gcn_fix_device_async(struct joybus *bus, uint16_t wireless_id, struct joybus_id *response,
                                joybus_transfer_cb_t callback, void *user_data)
{
  // Build the command
  bus->command_buffer[0] = JOYBUS_CMD_GCN_FIX_DEVICE;
  bus->command_buffer[1] = ((wireless_id >> 2) & 0xC0) | 0x10;
  bus->command_buffer[2] = wireless_id & 0xFF;

  // Transfer the command and read the response directly into the response buffer
  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_GCN_FIX_DEVICE_TX, (uint8_t *)response,
                         JOYBUS_CMD_GCN_FIX_DEVICE_RX, callback, user_data);
}
