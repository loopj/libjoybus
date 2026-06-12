#include <stdbool.h>
#include <string.h>

#include <joybus/checksum.h>
#include <joybus/commands.h>
#include <joybus/errors.h>
#include <joybus/identify.h>
#include <joybus/target/n64_controller.h>

// Helper to check if an pak is currently read for pak read/write commands
// An pak is "ready" when it is present and pak changed flag has been cleared
static inline bool pak_ready(struct joybus_target_n64_controller *controller)
{
  return controller->pak && !joybus_id_n64_pak_changed(&controller->id);
}

/**
 * Handle "reset" commands.
 *
 * Command:         {0xFF}
 * Response:        A 3-byte controller ID
 */
static int handle_reset(struct joybus_target_n64_controller *controller, const uint8_t *command, uint8_t bytes_read,
                        joybus_target_response_cb_t send_response, void *user_data)
{
  // Respond with the controller ID
  send_response((uint8_t *)&controller->id, JOYBUS_CMD_RESET_RX, user_data);

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
static int handle_identify(struct joybus_target_n64_controller *controller, const uint8_t *command, uint8_t bytes_read,
                           joybus_target_response_cb_t send_response, void *user_data)
{
  // Snapshot the controller ID into the response buffer so id can be mutated below
  memcpy(controller->response, &controller->id, sizeof(controller->id));

  // Respond with the controller ID
  send_response(controller->response, JOYBUS_CMD_IDENTIFY_RX, user_data);

  // After reporting "pak changed", transition to "pak present"
  if (joybus_id_n64_pak_changed(&controller->id))
    joybus_id_clear_status_flags(&controller->id, JOYBUS_STATUS_N64_PAK_PULLED);

  // Clear any latched checksum error — reported once, then cleared
  joybus_id_clear_status_flags(&controller->id, JOYBUS_STATUS_N64_ADDR_CHECKSUM_ERROR);

  return 0;
}

/**
 * Handle "read" commands, to fetch the current input state.
 *
 * Command:         {0x01}
 * Response:        An 4-byte input state.
 */
static int handle_read(struct joybus_target_n64_controller *controller, const uint8_t *command, uint8_t bytes_read,
                       joybus_target_response_cb_t send_response, void *user_data)
{
  // Respond with the controller input state
  send_response((uint8_t *)&controller->input, JOYBUS_CMD_N64_READ_RX, user_data);

  return 0;
}

/**
 * Handle "pak read" commands, to read data from a connected pak such as a rumble pak or memory pak.
 *
 * Command:         {0x02, address[2]}
 * Response:        32 bytes of pak data, and a 1-byte CRC8
 *
 */
static int handle_pak_read(struct joybus_target_n64_controller *controller, const uint8_t *command, uint8_t bytes_read,
                           joybus_target_response_cb_t send_response, void *user_data)
{
  // Wait until the full command is received before responding
  if (bytes_read < JOYBUS_CMD_N64_PAK_READ_TX)
    return JOYBUS_CMD_N64_PAK_READ_TX - bytes_read;

  // Extract the address from the command
  uint16_t addr = ((uint16_t)command[1] << 8) | command[2];

  // Validate the address checksum
  bool checksum_valid = joybus_address_checksum(addr >> 5) == (addr & 0x1F);

  // Set or clear the checksum error flag in the controller ID
  if (checksum_valid) {
    joybus_id_clear_status_flags(&controller->id, JOYBUS_STATUS_N64_ADDR_CHECKSUM_ERROR);
  } else {
    joybus_id_set_status_flags(&controller->id, JOYBUS_STATUS_N64_ADDR_CHECKSUM_ERROR);
  }

  // Prepare and send the response
  if (pak_ready(controller) && checksum_valid) {
    // Ask the pak to fill the response buffer
    uint16_t block_addr               = addr & 0xFFE0;
    struct joybus_target_n64_pak *acc = controller->pak;
    acc->api->read_block(acc, block_addr, controller->response);

    // Calculate and append the data checksum
    controller->response[JOYBUS_PAK_BLOCK_SIZE] = joybus_data_checksum(controller->response, JOYBUS_PAK_BLOCK_SIZE);

    // Send the response
    send_response(controller->response, JOYBUS_CMD_N64_PAK_READ_RX, user_data);
  } else {
    // Prepare a zero response with the "no pak" CRC
    memset(controller->response, 0, JOYBUS_CMD_N64_PAK_READ_RX);
    controller->response[JOYBUS_PAK_BLOCK_SIZE] = 0xFF;

    // Send the response
    send_response(controller->response, JOYBUS_CMD_N64_PAK_READ_RX, user_data);
  }

  return 0;
}

/**
 * Handle "pak write" commands, to write data to a connected pak such as a rumble pak or memory pak.
 *
 * Command:         {0x03, address[2], data[32]}
 * Response:        A 1-byte CRC8 of the written data
 *
 */
static int handle_pak_write(struct joybus_target_n64_controller *controller, const uint8_t *command, uint8_t bytes_read,
                            joybus_target_response_cb_t send_response, void *user_data)
{
  // First 3 bytes are command and address
  if (bytes_read == 3) {
    // Extract the address from the command
    uint16_t addr = ((uint16_t)command[1] << 8) | command[2];

    // Set or clear the checksum error flag
    if (joybus_address_checksum(addr >> 5) == (addr & 0x1F)) {
      joybus_id_clear_status_flags(&controller->id, JOYBUS_STATUS_N64_ADDR_CHECKSUM_ERROR);
    } else {
      joybus_id_set_status_flags(&controller->id, JOYBUS_STATUS_N64_ADDR_CHECKSUM_ERROR);
    }

    // Reset the running CRC for the payload bytes
    controller->crc = 0;

    return JOYBUS_CMD_N64_PAK_WRITE_TX - bytes_read;
  }

  // Subsequent bytes are payload, accumulate the checksum
  controller->crc = joybus_data_checksum_update(controller->crc, command[bytes_read - 1]);

  // Full payload received, respond with the CRC
  if (bytes_read == JOYBUS_CMD_N64_PAK_WRITE_TX) {
    bool checksum_valid = (joybus_id_get_status(&controller->id) & JOYBUS_STATUS_N64_ADDR_CHECKSUM_ERROR) == 0;
    bool ready          = pak_ready(controller) && checksum_valid;

    // Mark the CRC as "no pak" if we're not ready to commit the write
    if (!ready) {
      controller->crc ^= 0xFF;
    }

    // Send the CRC response first to keep the storage write off the response critical path
    send_response(&controller->crc, JOYBUS_CMD_N64_PAK_WRITE_RX, user_data);

    // Hand the payload to the pak after the host has its response
    if (ready) {
      uint16_t addr                     = ((uint16_t)command[1] << 8) | command[2];
      uint16_t block_addr               = addr & 0xFFE0;
      struct joybus_target_n64_pak *acc = controller->pak;
      acc->api->write_block(acc, block_addr, &command[3]);
    }
  }

  return JOYBUS_CMD_N64_PAK_WRITE_TX - bytes_read;
}

static int n64_controller_byte_received(struct joybus_target *target, const uint8_t *command, uint8_t bytes_read,
                                        joybus_target_response_cb_t send_response, void *user_data)
{
  struct joybus_target_n64_controller *controller = JOYBUS_TARGET_N64_CONTROLLER(target);
  switch (command[0]) {
    case JOYBUS_CMD_RESET:
      return handle_reset(controller, command, bytes_read, send_response, user_data);
    case JOYBUS_CMD_IDENTIFY:
      return handle_identify(controller, command, bytes_read, send_response, user_data);
    case JOYBUS_CMD_N64_READ:
      return handle_read(controller, command, bytes_read, send_response, user_data);
    case JOYBUS_CMD_N64_PAK_READ:
      return handle_pak_read(controller, command, bytes_read, send_response, user_data);
    case JOYBUS_CMD_N64_PAK_WRITE:
      return handle_pak_write(controller, command, bytes_read, send_response, user_data);
  }

  return -JOYBUS_ERR_NOT_SUPPORTED;
}

static const struct joybus_target_api n64_controller_api = {
  .byte_received = n64_controller_byte_received,
};

void joybus_target_n64_controller_init(struct joybus_target_n64_controller *controller)
{
  // Start from a clean state
  memset(controller, 0, sizeof(*controller));

  // Set the target callbacks
  struct joybus_target *target = JOYBUS_TARGET(controller);
  target->api                  = &n64_controller_api;

  // Initialize the controller ID
  joybus_id_set_type_flags(&controller->id, JOYBUS_DEVICE_N64_CONTROLLER);
  joybus_id_set_status_flags(&controller->id, JOYBUS_STATUS_N64_PAK_PULLED);
}

void joybus_target_n64_controller_set_reset_cb(struct joybus_target_n64_controller *controller,
                                               joybus_target_n64_controller_reset_cb_t callback)
{
  controller->on_reset = callback;
}

void joybus_target_n64_controller_attach_pak(struct joybus_target_n64_controller *controller,
                                             struct joybus_target_n64_pak *pak)
{
  // Set the pak pointer on the controller
  controller->pak = pak;

  joybus_id_clear_status_flags(&controller->id, JOYBUS_STATUS_N64_PAK_PRESENT | JOYBUS_STATUS_N64_PAK_PULLED);
  if (joybus_target_is_registered(JOYBUS_TARGET(controller))) {
    joybus_id_set_status_flags(&controller->id, JOYBUS_STATUS_N64_PAK_PRESENT | JOYBUS_STATUS_N64_PAK_PULLED);
  } else {
    joybus_id_set_status_flags(&controller->id, JOYBUS_STATUS_N64_PAK_PRESENT);
  }
}

void joybus_target_n64_controller_detach_pak(struct joybus_target_n64_controller *controller)
{
  // Clear the pak pointer on the controller
  controller->pak = NULL;

  // Mark the pak as absent
  joybus_id_clear_status_flags(&controller->id, JOYBUS_STATUS_N64_PAK_PRESENT);
  joybus_id_set_status_flags(&controller->id, JOYBUS_STATUS_N64_PAK_PULLED);
}
