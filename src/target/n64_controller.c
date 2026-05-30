#include <stdbool.h>
#include <string.h>

#include <joybus/checksum.h>
#include <joybus/commands.h>
#include <joybus/errors.h>
#include <joybus/target/n64_controller.h>

// Helper to check if an accessory is currently read for accessory read/write commands
// An accessory is "ready" when it is present and accessory changed flag has been cleared
static inline bool accessory_ready(struct joybus_n64_controller *controller)
{
  uint8_t status = joybus_id_get_status(controller->id);
  return controller->accessory && (status & JOYBUS_ID_N64_ACCESSORY_CHANGED) != JOYBUS_ID_N64_ACCESSORY_CHANGED;
}

/**
 * Handle "reset" commands.
 *
 * Command:         {0xFF}
 * Response:        A 3-byte controller ID
 */
static int handle_reset(struct joybus_n64_controller *controller, const uint8_t *command, uint8_t bytes_read,
                        joybus_target_response_cb_t send_response, void *user_data)
{
  // Respond with the controller ID
  send_response(controller->id, JOYBUS_CMD_RESET_RX, user_data);

  // Call the reset callback if it exists
  if (controller->on_reset)
    controller->on_reset(controller);

  return 0;
}

/**
 * Handle "identify" commands.
 *
 * Command:         {0x00}
 * Response:        A 3-byte controller ID
 */
static int handle_identify(struct joybus_n64_controller *controller, const uint8_t *command, uint8_t bytes_read,
                           joybus_target_response_cb_t send_response, void *user_data)
{
  // Snapshot the controller ID into the response buffer so id can be mutated below
  controller->response[0] = controller->id[0];
  controller->response[1] = controller->id[1];
  controller->response[2] = controller->id[2];

  // Respond with the controller ID
  send_response(controller->response, JOYBUS_CMD_IDENTIFY_RX, user_data);

  // After reporting "accessory changed", transition to "accessory present"
  uint8_t status = joybus_id_get_status(controller->id);
  if ((status & JOYBUS_ID_N64_ACCESSORY_CHANGED) == JOYBUS_ID_N64_ACCESSORY_CHANGED)
    joybus_id_clear_status_flags(controller->id, JOYBUS_ID_N64_ACCESSORY_ABSENT);

  // Clear any latched checksum error — reported once, then cleared
  joybus_id_clear_status_flags(controller->id, JOYBUS_ID_N64_CHECKSUM_ERROR);

  return 0;
}

/**
 * Handle "read" commands, to fetch the current input state.
 *
 * Command:         {0x01}
 * Response:        An 4-byte input state.
 */
static int handle_read(struct joybus_n64_controller *controller, const uint8_t *command, uint8_t bytes_read,
                       joybus_target_response_cb_t send_response, void *user_data)
{
  // Respond with the controller input state
  send_response((uint8_t *)&controller->input, JOYBUS_CMD_N64_READ_RX, user_data);

  return 0;
}

/**
 * Handle "accessory read" commands, to read data from a connected accessory such as a rumble pak or memory pak.
 *
 * Command:         {0x02, address[2]}
 * Response:        32 bytes of accessory data, and a 1-byte CRC8
 *
 */
static int handle_accessory_read(struct joybus_n64_controller *controller, const uint8_t *command, uint8_t bytes_read,
                                 joybus_target_response_cb_t send_response, void *user_data)
{
  // Wait until the full command is received before responding
  if (bytes_read < JOYBUS_CMD_N64_ACCESSORY_READ_TX)
    return JOYBUS_CMD_N64_ACCESSORY_READ_TX - bytes_read;

  // Extract the address from the command
  uint16_t addr = ((uint16_t)command[1] << 8) | command[2];

  // Validate the address checksum
  bool checksum_valid = joybus_address_checksum_valid(addr);

  // Set or clear the checksum error flag in the controller ID
  if (checksum_valid) {
    joybus_id_clear_status_flags(controller->id, JOYBUS_ID_N64_CHECKSUM_ERROR);
  } else {
    joybus_id_set_status_flags(controller->id, JOYBUS_ID_N64_CHECKSUM_ERROR);
  }

  // Prepare and send the response
  if (accessory_ready(controller) && checksum_valid) {
    // Ask the accessory to fill the response buffer
    uint16_t block_addr              = addr & 0xFFE0;
    struct joybus_n64_accessory *acc = controller->accessory;
    acc->api->read_block(acc, block_addr, controller->response);

    // Calculate and append the CRC8
    controller->response[JOYBUS_ACCESSORY_BLOCK_SIZE] = joybus_crc8(controller->response, JOYBUS_ACCESSORY_BLOCK_SIZE);

    // Send the response
    send_response(controller->response, JOYBUS_CMD_N64_ACCESSORY_READ_RX, user_data);
  } else {
    // Prepare a zero response with the "no accessory" CRC
    memset(controller->response, 0, JOYBUS_CMD_N64_ACCESSORY_READ_RX);
    controller->response[JOYBUS_ACCESSORY_BLOCK_SIZE] = 0xFF;

    // Send the response
    send_response(controller->response, JOYBUS_CMD_N64_ACCESSORY_READ_RX, user_data);
  }

  return 0;
}

/**
 * Handle "accessory write" commands, to write data to a connected accessory such as a rumble pak or memory pak.
 *
 * Command:         {0x03, address[2], data[32]}
 * Response:        A 1-byte CRC8 of the written data
 *
 */
static int handle_accessory_write(struct joybus_n64_controller *controller, const uint8_t *command, uint8_t bytes_read,
                                  joybus_target_response_cb_t send_response, void *user_data)
{
  // First 3 bytes are command and address
  if (bytes_read == 3) {
    // Extract the address from the command
    uint16_t addr = ((uint16_t)command[1] << 8) | command[2];

    // Set or clear the checksum error flag
    if (joybus_address_checksum_valid(addr)) {
      joybus_id_clear_status_flags(controller->id, JOYBUS_ID_N64_CHECKSUM_ERROR);
    } else {
      joybus_id_set_status_flags(controller->id, JOYBUS_ID_N64_CHECKSUM_ERROR);
    }

    // Reset the running CRC for the payload bytes
    controller->crc = 0;

    return JOYBUS_CMD_N64_ACCESSORY_WRITE_TX - bytes_read;
  }

  // Subsequent bytes are payload, accumulate the CRC
  controller->crc = joybus_crc8_update(controller->crc, command[bytes_read - 1]);

  // Full payload received, respond with the CRC
  if (bytes_read == JOYBUS_CMD_N64_ACCESSORY_WRITE_TX) {
    bool checksum_valid = (joybus_id_get_status(controller->id) & JOYBUS_ID_N64_CHECKSUM_ERROR) == 0;
    bool ready          = accessory_ready(controller) && checksum_valid;

    // Mark the CRC as "no accessory" if we're not ready to commit the write
    if (!ready) {
      controller->crc ^= 0xFF;
    }

    // Send the CRC response first to keep the storage write off the response critical path
    send_response(&controller->crc, JOYBUS_CMD_N64_ACCESSORY_WRITE_RX, user_data);

    // Hand the payload to the accessory after the host has its response
    if (ready) {
      uint16_t addr                    = ((uint16_t)command[1] << 8) | command[2];
      uint16_t block_addr              = addr & 0xFFE0;
      struct joybus_n64_accessory *acc = controller->accessory;
      acc->api->write_block(acc, block_addr, &command[3]);
    }
  }

  return JOYBUS_CMD_N64_ACCESSORY_WRITE_TX - bytes_read;
}

static int n64_controller_byte_received(struct joybus_target *target, const uint8_t *command, uint8_t bytes_read,
                                        joybus_target_response_cb_t send_response, void *user_data)
{
  struct joybus_n64_controller *controller = JOYBUS_N64_CONTROLLER(target);
  switch (command[0]) {
    case JOYBUS_CMD_RESET:
      return handle_reset(controller, command, bytes_read, send_response, user_data);
    case JOYBUS_CMD_IDENTIFY:
      return handle_identify(controller, command, bytes_read, send_response, user_data);
    case JOYBUS_CMD_N64_READ:
      return handle_read(controller, command, bytes_read, send_response, user_data);
    case JOYBUS_CMD_N64_ACCESSORY_READ:
      return handle_accessory_read(controller, command, bytes_read, send_response, user_data);
    case JOYBUS_CMD_N64_ACCESSORY_WRITE:
      return handle_accessory_write(controller, command, bytes_read, send_response, user_data);
  }

  return -JOYBUS_ERR_NOT_SUPPORTED;
}

static const struct joybus_target_api n64_controller_api = {
  .byte_received = n64_controller_byte_received,
};

void joybus_n64_controller_init(struct joybus_n64_controller *controller)
{
  // Start from a clean state
  memset(controller, 0, sizeof(*controller));

  // Set the target callbacks
  struct joybus_target *target = JOYBUS_TARGET(controller);
  target->api                  = &n64_controller_api;

  // Initialize the controller ID
  joybus_id_set_type_flags(controller->id, JOYBUS_ID_N64_CONTROLLER);
  joybus_id_set_status_flags(controller->id, JOYBUS_ID_N64_ACCESSORY_ABSENT);
}

void joybus_n64_controller_set_reset_callback(struct joybus_n64_controller *controller,
                                              joybus_n64_controller_reset_cb_t callback)
{
  controller->on_reset = callback;
}

void joybus_n64_controller_attach_accessory(struct joybus_n64_controller *controller,
                                            struct joybus_n64_accessory *accessory)
{
  // Set the accessory pointer on the controller
  controller->accessory = accessory;

  joybus_id_clear_status_flags(controller->id, JOYBUS_ID_N64_ACCESSORY_CHANGED);
  if (joybus_target_is_registered(JOYBUS_TARGET(controller))) {
    joybus_id_set_status_flags(controller->id, JOYBUS_ID_N64_ACCESSORY_CHANGED);
  } else {
    joybus_id_set_status_flags(controller->id, JOYBUS_ID_N64_ACCESSORY_PRESENT);
  }
}

void joybus_n64_controller_detach_accessory(struct joybus_n64_controller *controller)
{
  // Clear the accessory pointer on the controller
  controller->accessory = NULL;

  // Mark the accessory as absent
  joybus_id_clear_status_flags(controller->id, JOYBUS_ID_N64_ACCESSORY_CHANGED);
  joybus_id_set_status_flags(controller->id, JOYBUS_ID_N64_ACCESSORY_ABSENT);
}