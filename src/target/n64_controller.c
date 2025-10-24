#include <joybus/commands.h>
#include <joybus/errors.h>
#include <joybus/target/n64_controller.h>

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
  // Respond with the controller ID
  send_response(controller->id, JOYBUS_CMD_IDENTIFY_RX, user_data);

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
  }

  return -JOYBUS_ERR_NOT_SUPPORTED;
}

static const struct joybus_target_api n64_controller_api = {
  .byte_received = n64_controller_byte_received,
};

void joybus_n64_controller_init(struct joybus_n64_controller *controller, uint8_t type)
{
  // Set the target callbacks
  struct joybus_target *target = JOYBUS_TARGET(controller);
  target->api                  = &n64_controller_api;

  // Initialize the controller ID
  controller->id[0] = type;
  controller->id[1] = 0x00;
  controller->id[2] = 0x00;
}