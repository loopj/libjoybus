/**
 * N64 host functions.
 *
 * The accessory detection logic is borrowed heavily from libdragon.
 */

#include <string.h>

#include <joybus/commands.h>
#include <joybus/crc8.h>
#include <joybus/host/n64.h>

#define ACCESSORY_ADDR_LABEL              0x0000
#define ACCESSORY_ADDR_PROBE              0x8000
#define ACCESSORY_ADDR_RUMBLE_MOTOR       0xC000

#define ACCESSORY_PROBE_TYPE_RUMBLE_PAK   0x80
#define ACCESSORY_PROBE_TYPE_BIO_SENSOR   0x81
#define ACCESSORY_PROBE_TYPE_TRANSFER_PAK 0x84
#define ACCESSORY_PROBE_TYPE_SNAP_STATION 0x85
#define ACCESSORY_PROBE_TYPE_RESET        0xFE

// Accessory detection state machine steps
enum {
  DETECT_STEP_NONE = 0,
  DETECT_STEP_INIT,
  DETECT_STEP_CONTROLLER_PAK_RESET,
  DETECT_STEP_CONTROLLER_PAK_LABEL_BACKUP,
  DETECT_STEP_CONTROLLER_PAK_LABEL_OVERWRITE,
  DETECT_STEP_CONTROLLER_PAK_LABEL_TEST,
  DETECT_STEP_CONTROLLER_PAK_LABEL_RESTORE,
  DETECT_STEP_RUMBLE_PAK_PROBE_WRITE,
  DETECT_STEP_RUMBLE_PAK_PROBE_READ,
  DETECT_STEP_TRANSFER_PAK_PROBE_WRITE,
  DETECT_STEP_TRANSFER_PAK_PROBE_READ,
  DETECT_STEP_TRANSFER_PAK_TURN_OFF,
  DETECT_STEP_SNAP_STATION_PROBE_WRITE,
  DETECT_STEP_SNAP_STATION_PROBE_READ,
  DETECT_STEP_ERROR,
};

struct detection_state {
  uint8_t step;
  uint8_t response[JOYBUS_BLOCK_SIZE];
  uint8_t write_buf[32];
  uint8_t label_backup[32];

  joybus_n64_accessory_detect_cb_t user_callback;
  void *user_data;
};

// LUT for computing address checksum
static const uint8_t cs_tab[11] = {0x01, 0x1A, 0x0D, 0x1C, 0x0E, 0x07, 0x19, 0x16, 0x0B, 0x1F, 0x15};

// Compute address with checksum for N64 memory operations
static uint16_t address_with_checksum(uint16_t addr)
{
  uint8_t sum = 0;
  for (int i = 0; i < 11; i++) {
    if (addr & (1u << (15 - i)))
      sum ^= cs_tab[i];
  }

  return (addr & 0xFFE0) | (sum & 0x1F);
}

int joybus_n64_read(struct joybus *bus, uint8_t *response, joybus_transfer_cb_t callback, void *user_data)
{
  // Build command
  bus->command_buffer[0] = JOYBUS_CMD_N64_READ;

  // Send command
  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_N64_READ_TX, response, JOYBUS_CMD_N64_READ_RX, callback,
                         user_data);
}

int joybus_n64_accessory_write(struct joybus *bus, uint16_t addr, const uint8_t *data, uint8_t *response,
                               joybus_transfer_cb_t callback, void *user_data)
{
  // Generate address with checksum
  uint16_t with_checksum = address_with_checksum(addr);

  // Build command
  bus->command_buffer[0] = JOYBUS_CMD_N64_ACCESSORY_WRITE;
  bus->command_buffer[1] = (uint8_t)(with_checksum >> 8);
  bus->command_buffer[2] = (uint8_t)(with_checksum & 0xFF);

  // Copy data to be written
  memcpy(&bus->command_buffer[3], data, 32);

  // Send command
  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_N64_ACCESSORY_WRITE_TX, response,
                         JOYBUS_CMD_N64_ACCESSORY_WRITE_RX, callback, user_data);
}

int joybus_n64_accessory_read(struct joybus *bus, uint16_t addr, uint8_t *response, joybus_transfer_cb_t callback,
                              void *user_data)
{
  // Generate address with checksum
  uint16_t with_checksum = address_with_checksum(addr);

  // Build command
  bus->command_buffer[0] = JOYBUS_CMD_N64_ACCESSORY_READ;
  bus->command_buffer[1] = (uint8_t)(with_checksum >> 8);
  bus->command_buffer[2] = (uint8_t)(with_checksum & 0xFF);

  // Send command
  return joybus_transfer(bus, bus->command_buffer, JOYBUS_CMD_N64_ACCESSORY_READ_TX, response,
                         JOYBUS_CMD_N64_ACCESSORY_READ_RX, callback, user_data);
}

// Check (and respond) to failures during accessory detection write operations
static int validate_detection_write(struct detection_state *state)
{
  uint8_t expected_crc = si_crc8(state->write_buf, 32);
  if (state->response[0] == (expected_crc ^ 0xFF)) {
    state->user_callback(JOYBUS_N64_ACCESSORY_NONE, state->user_data);
    return -1;
  }

  if (state->response[0] != expected_crc) {
    state->user_callback(JOYBUS_N64_ACCESSORY_UNKNOWN, state->user_data);
    return -2;
  }

  // Valid CRC, write succeeded
  return 0;
}

// Check (and respond) to failures during accessory detection read operations
static int validate_detection_read(struct detection_state *state)
{
  uint8_t expected_crc = si_crc8(state->response, 32);
  if (state->response[32] == (expected_crc ^ 0xFF)) {
    state->user_callback(JOYBUS_N64_ACCESSORY_NONE, state->user_data);
    return -1;
  }

  if (state->response[32] != expected_crc) {
    state->user_callback(JOYBUS_N64_ACCESSORY_UNKNOWN, state->user_data);
    return -2;
  }

  // Valid CRC, read succeeded
  return 0;
}

// Callback for accessory detection state machine
static void accessory_detection_cb(struct joybus *bus, int result, void *user_data)
{
  struct detection_state *state = (struct detection_state *)user_data;

  // Handle transfer errors
  if (result < 0)
    return state->user_callback(JOYBUS_N64_ACCESSORY_UNKNOWN, state->user_data);

  // State machine
  switch (state->step) {
    case DETECT_STEP_INIT:
      if (validate_detection_write(state) < 0)
        return;

      // Move to the next step, reset the Controller Pak by writing all zeros to the probe address
      state->step = DETECT_STEP_CONTROLLER_PAK_RESET;
      memset(state->write_buf, 0x00, sizeof(state->write_buf));
      joybus_n64_accessory_write(bus, ACCESSORY_ADDR_PROBE, state->write_buf, state->response, accessory_detection_cb,
                                 state);
      break;

    case DETECT_STEP_CONTROLLER_PAK_RESET:
      if (validate_detection_write(state) < 0)
        return;

      // Move to the next step, read the current Controller Pak "label" area
      state->step = DETECT_STEP_CONTROLLER_PAK_LABEL_BACKUP;
      joybus_n64_accessory_read(bus, ACCESSORY_ADDR_LABEL, state->response, accessory_detection_cb, state);
      break;

    case DETECT_STEP_CONTROLLER_PAK_LABEL_BACKUP:
      if (validate_detection_read(state) < 0)
        return;

      // Backup the label area
      memcpy(state->label_backup, state->response, 32);

      // Move to the next step, overwrite the "label" area to detect Controller Pak
      state->step = DETECT_STEP_CONTROLLER_PAK_LABEL_OVERWRITE;
      for (int i = 0; i < 32; i++)
        state->write_buf[i] = (uint8_t)i;

      joybus_n64_accessory_write(bus, ACCESSORY_ADDR_LABEL, state->write_buf, state->response, accessory_detection_cb,
                                 state);
      break;

    case DETECT_STEP_CONTROLLER_PAK_LABEL_OVERWRITE:
      if (validate_detection_write(state) < 0)
        return;

      // Move to the next step, read back the "label" area to see if the write succeeded
      state->step = DETECT_STEP_CONTROLLER_PAK_LABEL_TEST;
      joybus_n64_accessory_read(bus, ACCESSORY_ADDR_LABEL, state->response, accessory_detection_cb, state);
      break;

    case DETECT_STEP_CONTROLLER_PAK_LABEL_TEST:
      if (validate_detection_read(state) < 0)
        return;

      // Check if the read data matches what we wrote
      if (memcmp(state->response, state->write_buf, sizeof(state->write_buf)) == 0) {
        // Move to the next step, restore the original label area
        state->step = DETECT_STEP_CONTROLLER_PAK_LABEL_RESTORE;
        memcpy(state->write_buf, state->label_backup, sizeof(state->label_backup));
        joybus_n64_accessory_write(bus, ACCESSORY_ADDR_LABEL, state->write_buf, state->response, accessory_detection_cb,
                                   state);
      } else {
        // Move to the next step, write probe value to detect Rumble Pak
        state->step = DETECT_STEP_RUMBLE_PAK_PROBE_WRITE;
        memset(state->write_buf, ACCESSORY_PROBE_TYPE_RUMBLE_PAK, sizeof(state->write_buf));
        joybus_n64_accessory_write(bus, ACCESSORY_ADDR_PROBE, state->write_buf, state->response, accessory_detection_cb,
                                   state);
      }
      break;

    case DETECT_STEP_CONTROLLER_PAK_LABEL_RESTORE:
      if (validate_detection_write(state) < 0)
        return;

      // Detected a Controller Pak
      return state->user_callback(JOYBUS_N64_ACCESSORY_CONTROLLER_PAK, state->user_data);

    case DETECT_STEP_RUMBLE_PAK_PROBE_WRITE:
      if (validate_detection_write(state) < 0)
        return;

      // Move to the next step, read probe value to detect Rumble Pak
      state->step = DETECT_STEP_RUMBLE_PAK_PROBE_READ;
      joybus_n64_accessory_read(bus, ACCESSORY_ADDR_PROBE, state->response, accessory_detection_cb, state);
      break;

    case DETECT_STEP_RUMBLE_PAK_PROBE_READ:
      if (validate_detection_read(state) < 0)
        return;

      // Detected a Rumble Pak
      if (state->response[0] == ACCESSORY_PROBE_TYPE_RUMBLE_PAK)
        return state->user_callback(JOYBUS_N64_ACCESSORY_RUMBLE_PAK, state->user_data);

      // Detected a Bio Sensor
      if (state->response[0] == ACCESSORY_PROBE_TYPE_BIO_SENSOR)
        return state->user_callback(JOYBUS_N64_ACCESSORY_BIO_SENSOR, state->user_data);

      // Move to the next step, write probe value to detect Transfer Pak
      state->step = DETECT_STEP_TRANSFER_PAK_PROBE_WRITE;
      memset(state->write_buf, ACCESSORY_PROBE_TYPE_TRANSFER_PAK, sizeof(state->write_buf));
      joybus_n64_accessory_write(bus, ACCESSORY_ADDR_PROBE, state->write_buf, state->response, accessory_detection_cb,
                                 state);
      break;

    case DETECT_STEP_TRANSFER_PAK_PROBE_WRITE:
      if (validate_detection_write(state) < 0)
        return;

      // Move to the next step, read probe value to detect Transfer Pak
      state->step = DETECT_STEP_TRANSFER_PAK_PROBE_READ;
      joybus_n64_accessory_read(bus, ACCESSORY_ADDR_PROBE, state->response, accessory_detection_cb, state);
      break;

    case DETECT_STEP_TRANSFER_PAK_PROBE_READ:
      if (validate_detection_read(state) < 0)
        return;

      if (state->response[0] == ACCESSORY_PROBE_TYPE_TRANSFER_PAK) {
        // Move to the next step, turn off Transfer Pak
        state->step = DETECT_STEP_TRANSFER_PAK_TURN_OFF;
        memset(state->write_buf, ACCESSORY_PROBE_TYPE_RESET, sizeof(state->write_buf));
        joybus_n64_accessory_write(bus, ACCESSORY_ADDR_PROBE, state->write_buf, state->response, accessory_detection_cb,
                                   state);
      } else {
        // Move to the next step, write probe value to detect Snap Station
        state->step = DETECT_STEP_SNAP_STATION_PROBE_WRITE;
        memset(state->write_buf, ACCESSORY_PROBE_TYPE_SNAP_STATION, sizeof(state->write_buf));
        joybus_n64_accessory_write(bus, ACCESSORY_ADDR_PROBE, state->write_buf, state->response, accessory_detection_cb,
                                   state);
      }
      break;

    case DETECT_STEP_TRANSFER_PAK_TURN_OFF:
      if (validate_detection_write(state) < 0)
        return;

      // Detected a Transfer Pak
      return state->user_callback(JOYBUS_N64_ACCESSORY_TRANSFER_PAK, state->user_data);

    case DETECT_STEP_SNAP_STATION_PROBE_WRITE:
      if (validate_detection_write(state) < 0)
        return;

      // Move to the next step, read probe value to detect Snap Station
      state->step = DETECT_STEP_SNAP_STATION_PROBE_READ;
      joybus_n64_accessory_read(bus, ACCESSORY_ADDR_PROBE, state->response, accessory_detection_cb, state);
      break;

    case DETECT_STEP_SNAP_STATION_PROBE_READ:
      if (validate_detection_read(state) < 0)
        return;

      // Detected a Snap Station
      if (state->response[0] == ACCESSORY_PROBE_TYPE_SNAP_STATION)
        return state->user_callback(JOYBUS_N64_ACCESSORY_SNAP_STATION, state->user_data);

      // At this point, we have no more detection techniques left
      return state->user_callback(JOYBUS_N64_ACCESSORY_UNKNOWN, state->user_data);

    default:
      // Shouldn't happen, but if it does, return unknown
      return state->user_callback(JOYBUS_N64_ACCESSORY_UNKNOWN, state->user_data);
  }
}

void joybus_n64_accessory_detect(struct joybus *bus, const joybus_n64_accessory_detect_cb_t callback, void *user_data)
{
  // TODO: Find a better storage location for state
  static struct detection_state state = {};
  state.step                          = DETECT_STEP_INIT;
  state.user_callback                 = callback;
  state.user_data                     = user_data;

  // Kick off accessory detection sequence by writing the reset value to the probe address
  memset(state.write_buf, ACCESSORY_PROBE_TYPE_RESET, sizeof(state.write_buf));
  joybus_n64_accessory_write(bus, ACCESSORY_ADDR_PROBE, state.write_buf, state.response, accessory_detection_cb,
                             &state);
}

void joybus_n64_motor_start(struct joybus *bus)
{
  static uint8_t write_buf[32];
  memset(write_buf, 0x01, sizeof(write_buf));
  joybus_n64_accessory_write(bus, ACCESSORY_ADDR_RUMBLE_MOTOR, write_buf, write_buf, NULL, NULL);
}

void joybus_n64_motor_stop(struct joybus *bus)
{
  static uint8_t write_buf[32];
  memset(write_buf, 0x00, sizeof(write_buf));
  joybus_n64_accessory_write(bus, ACCESSORY_ADDR_RUMBLE_MOTOR, write_buf, write_buf, NULL, NULL);
}