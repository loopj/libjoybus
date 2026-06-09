#include <string.h>

#include <joybus/checksum.h>
#include <joybus/commands.h>
#include <joybus/errors.h>

#include <joybus/host/n64_vru.h>

#define VRU_MAX_ATTEMPTS 4

// READ2 and READ36 use the same command format, but with different data lengths
static int vru_addressed_read(struct joybus *bus, uint8_t cmd, uint16_t addr, uint8_t *out, size_t data_len)
{
  // Build command
  bus->command_buffer[0] = cmd;
  bus->command_buffer[1] = addr >> 3;
  bus->command_buffer[2] = (addr << 5) | joybus_address_checksum(addr);

  // Attempt to send command up to 4 times
  for (int attempt = VRU_MAX_ATTEMPTS; attempt > 0; attempt--) {
    // Send command
    uint8_t response[data_len + 1];
    int result = joybus_transfer_sync(bus, bus->command_buffer, 3, response, data_len + 1);
    if (result < 0)
      return result;

    // Validate checksum
    uint8_t expected_checksum = response[data_len];
    if (expected_checksum == joybus_data_checksum(response, data_len)) {
      memcpy(out, response, data_len);
      return 0;
    }
  }

  return -JOYBUS_ERR_CHECKSUM;
}

// WRITE4 and WRITE20 use the same command format, but with different data lengths
static int vru_addressed_write(struct joybus *bus, uint8_t cmd, uint16_t addr, const uint8_t *data, size_t data_len)
{
  // Build command
  bus->command_buffer[0] = cmd;
  bus->command_buffer[1] = addr >> 3;
  bus->command_buffer[2] = (addr << 5) | joybus_address_checksum(addr);

  // Copy data to be written
  memcpy(&bus->command_buffer[3], data, data_len);

  // Attempt to send command up to 4 times
  for (int attempt = VRU_MAX_ATTEMPTS; attempt > 0; attempt--) {
    // Send command
    uint8_t expected_checksum;
    int result = joybus_transfer_sync(bus, bus->command_buffer, 3 + data_len, &expected_checksum, 1);
    if (result < 0)
      return result;

    // Verify checksum
    if (expected_checksum == joybus_data_checksum(data, data_len))
      return 0;
  }

  return -JOYBUS_ERR_CHECKSUM;
}

int joybus_n64_read36(struct joybus *bus, uint16_t addr, uint8_t data[36])
{
  return vru_addressed_read(bus, JOYBUS_CMD_N64_VRU_READ36, addr, data, 36);
}

int joybus_n64_vru_write20(struct joybus *bus, uint16_t addr, const uint8_t data[20])
{
  return vru_addressed_write(bus, JOYBUS_CMD_N64_VRU_WRITE20, addr, data, 20);
}

int joybus_n64_vru_read2(struct joybus *bus, uint16_t addr, uint8_t data[2])
{
  return vru_addressed_read(bus, JOYBUS_CMD_N64_VRU_READ2, addr, data, 2);
}

int joybus_n64_vru_write4(struct joybus *bus, uint16_t addr, const uint8_t data[4])
{
  return vru_addressed_write(bus, JOYBUS_CMD_N64_VRU_WRITE4, addr, data, 4);
}

int joybus_n64_vru_swrite(struct joybus *bus, uint8_t value)
{
  // Build the command
  bus->command_buffer[0] = JOYBUS_CMD_N64_VRU_SWRITE;
  bus->command_buffer[1] = value;
  bus->command_buffer[2] = joybus_address_checksum(value << 3);

  // Attempt to send command up to 4 times
  for (int attempt = VRU_MAX_ATTEMPTS; attempt > 0; attempt--) {
    // Send command
    uint8_t response;
    int result = joybus_transfer_sync(bus, bus->command_buffer, JOYBUS_CMD_N64_VRU_SWRITE_TX, &response,
                                      JOYBUS_CMD_N64_VRU_SWRITE_RX);
    if (result < 0)
      return result;

    // Success if the low bit of the response is clear, retry otherwise
    if ((response & 1) == 0)
      return 0;
  }

  return -JOYBUS_ERR_CHECKSUM;
}

int joybus_n64_vru_init(struct joybus *bus)
{
  int result;

  // Step 1: Initialize ADC
  static const uint8_t VRU_BRINGUP[] = {0x1E, 0x6E, 0x08, 0x56, 0x03};
  for (size_t i = 0; i < sizeof(VRU_BRINGUP); i++) {
    result = joybus_n64_vru_swrite(bus, VRU_BRINGUP[i]);
    if (result < 0)
      return result;
  }

  // Step 2: Initialize VRU
  uint8_t command[] = {0x00, 0x00, 0x01, 0x00};
  result = joybus_n64_vru_write4(bus, 0, command);
  if (result < 0)
    return result;

  // Step 3: Read VRU status
  uint8_t status[2];
  result = joybus_n64_vru_read2(bus, 0x0000, status);
  if (result < 0)
    return result;

  return 0;
}
