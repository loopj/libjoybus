#include <string.h>

#include <joybus/commands.h>
#include <joybus/host/gamecube.h>

int joybus_gcn_read(struct joybus *bus, enum joybus_gcn_analog_mode analog_mode,
                    enum joybus_gcn_motor_state motor_state, uint8_t *response, joybus_transfer_cb_t callback,
                    void *user_data)
{
  bus->command_buffer[0] = JOYBUS_CMD_GCN_READ;
  bus->command_buffer[1] = analog_mode;
  bus->command_buffer[2] = motor_state;

  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_GCN_READ_TX, response, JOYBUS_CMD_GCN_READ_RX, callback,
                         user_data);
}

int joybus_gcn_read_origin(struct joybus *bus, uint8_t *response, joybus_transfer_cb_t callback, void *user_data)
{
  bus->command_buffer[0] = JOYBUS_CMD_GCN_READ_ORIGIN;

  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_GCN_READ_ORIGIN_TX, response,
                         JOYBUS_CMD_GCN_READ_ORIGIN_RX, callback, user_data);
}

int joybus_gcn_calibrate(struct joybus *bus, uint8_t *response, joybus_transfer_cb_t callback, void *user_data)
{
  bus->command_buffer[0] = JOYBUS_CMD_GCN_CALIBRATE;
  bus->command_buffer[1] = 0;
  bus->command_buffer[2] = 0;

  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_GCN_CALIBRATE_TX, response, JOYBUS_CMD_GCN_CALIBRATE_RX,
                         callback, user_data);
}

int joybus_gcn_read_long(struct joybus *bus, enum joybus_gcn_motor_state motor_state, uint8_t *response,
                         joybus_transfer_cb_t callback, void *user_data)
{
  bus->command_buffer[0] = JOYBUS_CMD_GCN_READ_LONG;
  bus->command_buffer[1] = 0; // Analog mode ignored for full precision reads
  bus->command_buffer[2] = motor_state;

  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_GCN_READ_LONG_TX, response, JOYBUS_CMD_GCN_READ_LONG_RX,
                         callback, user_data);
}

int joybus_gcn_probe_device(struct joybus *bus, uint8_t *response, joybus_transfer_cb_t callback, void *user_data)
{
  bus->command_buffer[0] = JOYBUS_CMD_GCN_PROBE_DEVICE;
  bus->command_buffer[1] = 0;
  bus->command_buffer[2] = 0;

  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_GCN_PROBE_DEVICE_TX, response,
                         JOYBUS_CMD_GCN_PROBE_DEVICE_RX, callback, user_data);
}

int joybus_gcn_fix_device(struct joybus *bus, uint16_t wireless_id, uint8_t *response, joybus_transfer_cb_t callback,
                          void *user_data)
{
  bus->command_buffer[0] = JOYBUS_CMD_GCN_FIX_DEVICE;
  bus->command_buffer[1] = ((wireless_id >> 2) & 0xC0) | 0x10;
  bus->command_buffer[2] = wireless_id & 0xFF;

  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_GCN_FIX_DEVICE_TX, response, JOYBUS_CMD_GCN_FIX_DEVICE_RX,
                         callback, user_data);
}

int joybus_gcn_unpack_input(struct joybus_gc_controller_input *dest, const uint8_t *src,
                            enum joybus_gcn_analog_mode analog_mode)
{
  // Copy the button and stick data
  memcpy(dest, src, 4);

  // Unpack the remaining analog input data
  switch (analog_mode) {
    default:
      // Substick X/Y full precision, triggers and analog A/B truncated to 4 bits
      dest->substick_x    = src[4];
      dest->substick_y    = src[5];
      dest->trigger_left  = (src[6] & 0xF0);
      dest->trigger_right = (src[6] & 0x0F) << 4;
      dest->analog_a      = (src[7] & 0xF0);
      dest->analog_b      = (src[7] & 0x0F) << 4;
      break;
    case JOYBUS_GCN_ANALOG_MODE_1:
      // Triggers full precision, substick X/Y and analog A/B truncated to 4 bits
      // src[4] = substick X/Y, high nibble = X, low nibble = Y
      dest->substick_x    = (src[4] & 0xF0);
      dest->substick_y    = (src[4] & 0x0F) << 4;
      dest->trigger_left  = src[5];
      dest->trigger_right = src[6];
      dest->analog_a      = (src[7] & 0xF0);
      dest->analog_b      = (src[7] & 0x0F) << 4;
      break;
    case JOYBUS_GCN_ANALOG_MODE_2:
      // Analog A/B full precision, substick X/Y and triggers truncated to 4 bits
      dest->substick_x    = (src[4] & 0xF0);
      dest->substick_y    = (src[4] & 0x0F) << 4;
      dest->trigger_left  = (src[5] & 0xF0);
      dest->trigger_right = (src[5] & 0x0F) << 4;
      dest->analog_a      = src[6];
      dest->analog_b      = src[7];
      break;
    case JOYBUS_GCN_ANALOG_MODE_3:
      // Substick X/Y and triggers full precision, analog A/B omitted;
      dest->substick_x    = src[4];
      dest->substick_y    = src[5];
      dest->trigger_left  = src[6];
      dest->trigger_right = src[7];
      break;
    case JOYBUS_GCN_ANALOG_MODE_4:
      // Substick X/Y and analog A/B full precision, triggers omitted
      dest->substick_x = src[4];
      dest->substick_y = src[5];
      dest->analog_a   = src[6];
      dest->analog_b   = src[7];
      break;
  }

  return 0;
}