#include <string.h>

#include <joybus/commands.h>
#include <joybus/errors.h>
#include <joybus/target/gc_controller.h>

/*
 * Pack an "full" input state into a "short" input state, depending on the analog mode.
 *
 * The "read" command used by games expects 8-byte responses, this is presumably
 * so it fit in a nice round multiple of 32-bit words.
 *
 * The full input state is 10 bytes long, so there are various ways to "pack" the input
 * state into 8 bytes. Depending on the analog mode, either one pair of analog inputs
 * can be omitted, or two pairs of analog inputs can be truncated to 4 bits.
 *
 * All production games, with the exception of Luigi's Mansion, use analog mode 3. This
 * mode omits the analog A/B inputs, and sends the substick X/Y and triggers at full
 * precision. Analog A/B buttons were only present in pre-production GameCube
 * controllers.
 */
static inline uint8_t *pack_input_state(uint8_t *dest, const struct joybus_gc_controller_input *src,
                                        enum joybus_gcn_analog_mode analog_mode)
{
  // Copy the button and stick data
  memcpy(dest, src, 4);

  // Pack the remaining analog input data
  switch (analog_mode) {
    default:
      // Substick X/Y full precision, triggers and analog A/B truncated to 4 bits
      dest[4] = src->substick_x;
      dest[5] = src->substick_y;
      dest[6] = (src->trigger_left & 0xF0) | (src->trigger_right >> 4);
      dest[7] = (src->analog_a & 0xF0) | (src->analog_b >> 4);
      break;
    case JOYBUS_GCN_ANALOG_MODE_1:
      // Triggers full precision, substick X/Y and analog A/B truncated to 4 bits
      dest[4] = (src->substick_x & 0xF0) | (src->substick_y >> 4);
      dest[5] = src->trigger_left;
      dest[6] = src->trigger_right;
      dest[7] = (src->analog_a & 0xF0) | (src->analog_b >> 4);
      break;
    case JOYBUS_GCN_ANALOG_MODE_2:
      // Analog A/B full precision, substick X/Y and triggers truncated to 4 bits
      dest[4] = (src->substick_x & 0xF0) | (src->substick_y >> 4);
      dest[5] = (src->trigger_left & 0xF0) | (src->trigger_right >> 4);
      dest[6] = src->analog_a;
      dest[7] = src->analog_b;
      break;
    case JOYBUS_GCN_ANALOG_MODE_3:
      // Substick X/Y and triggers full precision, analog A/B omitted
      dest[4] = src->substick_x;
      dest[5] = src->substick_y;
      dest[6] = src->trigger_left;
      dest[7] = src->trigger_right;
      break;
    case JOYBUS_GCN_ANALOG_MODE_4:
      // Substick X/Y and analog A/B full precision, triggers omitted
      dest[4] = src->substick_x;
      dest[5] = src->substick_y;
      dest[6] = src->analog_a;
      dest[7] = src->analog_b;
      break;
  }

  return dest;
}

// Set or clear the "need origin" flag in the input state and device ID
static inline void set_need_origin(struct joybus_gc_controller *controller, bool need_origin)
{
  // Always set the need_origin flag in the input state
  if (need_origin) {
    controller->input.buttons |= JOYBUS_GCN_NEED_ORIGIN;
  } else {
    controller->input.buttons &= ~JOYBUS_GCN_NEED_ORIGIN;
  }

  // Also update the device ID for non-wireless controllers
  if (!joybus_gc_controller_is_wireless(controller)) {
    if (need_origin) {
      joybus_id_set_status_flags(controller->id, JOYBUS_ID_GCN_NEED_ORIGIN);
    } else {
      joybus_id_clear_status_flags(controller->id, JOYBUS_ID_GCN_NEED_ORIGIN);
    }
  }
}

/**
 * Handle "reset" commands.
 *
 * Command:         {0xFF}
 * Response:        A 3-byte controller ID
 */
static inline int handle_reset(struct joybus_gc_controller *controller, const uint8_t *command, uint8_t bytes_read,
                               joybus_target_response_cb_t send_response, void *user_data)
{
  // Respond with the controller ID
  send_response(controller->id, JOYBUS_CMD_RESET_RX, user_data);

  // Call the reset callback if it exists
  if (controller->on_reset)
    controller->on_reset(controller);

  // Stop the rumble motor
  // TODO

  return 0;
}

/**
 * Handle "identify" commands.
 *
 * Command:         {0x00}
 * Response:        A 3-byte controller ID
 */
static inline int handle_identify(struct joybus_gc_controller *controller, const uint8_t *command, uint8_t bytes_read,
                                  joybus_target_response_cb_t send_response, void *user_data)
{
  // Respond with the controller ID
  send_response(controller->id, JOYBUS_CMD_IDENTIFY_RX, user_data);

  return 0;
}

/**
 * Handle "read" commands, to fetch the current input state.
 *
 * Command:         {0x40, analog_mode, motor_state}
 * Response:        An 8-byte packed input state, see `pack_input_state` for details
 */
static inline int handle_read(struct joybus_gc_controller *controller, const uint8_t *command, uint8_t bytes_read,
                              joybus_target_response_cb_t send_response, void *user_data)
{
  // We can respond after the first two bytes
  if (bytes_read == 2) {
    // If the input state is valid, use that for the response, otherwise use the origin
    struct joybus_gc_controller_input *input = controller->input_valid ? &controller->input : &controller->origin;

    // Respond with the appropriate input state
    // Most games use analog mode 3, which is just the first 8 bytes of the full input state
    // Otherwise, pack the input state based on the analog mode
    enum joybus_gcn_analog_mode analog_mode = command[1];
    if (analog_mode == JOYBUS_GCN_ANALOG_MODE_3) {
      send_response((uint8_t *)input, JOYBUS_CMD_GCN_READ_RX, user_data);
    } else {
      send_response(pack_input_state(controller->packed_input, input, analog_mode), JOYBUS_CMD_GCN_READ_RX, user_data);
    }
  } else if (bytes_read == JOYBUS_CMD_GCN_READ_TX) {
    // Save origin flags and state
    enum joybus_gcn_analog_mode analog_mode = command[1];
    enum joybus_gcn_motor_state motor_state = command[2];
    if (!joybus_gc_controller_is_wireless(controller)) {
      // Update the origin flags
      controller->input.buttons |= JOYBUS_GCN_USE_ORIGIN;

      // Save the analog mode and motor state
      joybus_id_clear_status_flags(controller->id, JOYBUS_ID_GCN_MOTOR_STATE_MASK | JOYBUS_ID_GCN_ANALOG_MODE_MASK);
      joybus_id_set_status_flags(controller->id, motor_state << JOYBUS_ID_GCN_MOTOR_STATE_SHIFT | analog_mode);
    }

    // If motor state has changed, call the motor state change callback
    // TODO
  }

  return JOYBUS_CMD_GCN_READ_TX - bytes_read;
}

/**
 * Handle "read origin" commands.
 *
 * Command:         {0x41}
 * Response:        A 10-byte input state representing the current origin.
 */
static inline int handle_read_origin(struct joybus_gc_controller *controller, const uint8_t *command,
                                     uint8_t bytes_read, joybus_target_response_cb_t send_response, void *user_data)
{
  // Respond with the controller origin
  send_response((uint8_t *)&controller->origin, JOYBUS_CMD_GCN_READ_ORIGIN_RX, user_data);

  // Clear the "need origin" flag
  set_need_origin(controller, false);

  return 0;
}

/**
 * Handle "calibrate" commands.
 *
 * Command:         {0x42, 0x00, 0x00}
 * Response:        A 10-byte input state representing the current origin.
 */
static inline int handle_calibrate(struct joybus_gc_controller *controller, const uint8_t *command, uint8_t bytes_read,
                                   joybus_target_response_cb_t send_response, void *user_data)
{
  // We can respond after the first byte
  if (bytes_read == 1) {
    // Set the current input state as the origin
    memcpy(&controller->origin, &controller->input, sizeof(controller->origin));

    // Respond with the new controller origin
    send_response((uint8_t *)&controller->origin, JOYBUS_CMD_GCN_CALIBRATE_RX, user_data);

    // Clear the "need origin" flag
    set_need_origin(controller, false);
  }

  return JOYBUS_CMD_GCN_CALIBRATE_TX - bytes_read;
}

/**
 * Handle "long read" commands, to fetch the current input state with full precision.
 *
 * Command Format:  {0x43, analog_mode, motor_state}
 * Response:        A 10-byte input state.
 *
 * NOTE: This command is not used by any games, but is included for completeness.
 */
static inline int handle_read_long(struct joybus_gc_controller *controller, const uint8_t *command, uint8_t bytes_read,
                                   joybus_target_response_cb_t send_response, void *user_data)
{
  // We can respond after the second byte is read
  if (bytes_read == 2) {
    // If the input state is valid, use that for the response, otherwise use the origin
    struct joybus_gc_controller_input *input = controller->input_valid ? &controller->input : &controller->origin;

    // Respond with the appropriate input state
    send_response((uint8_t *)input, JOYBUS_CMD_GCN_READ_LONG_RX, user_data);
  } else if (bytes_read == JOYBUS_CMD_GCN_READ_LONG_TX) {
    // Extract the analog mode and motor state from the command
    enum joybus_gcn_analog_mode analog_mode = command[1] & 0x07;
    enum joybus_gcn_motor_state motor_state = command[2] & 0x03;

    // Save origin flags and state
    if (!joybus_gc_controller_is_wireless(controller)) {
      // Update the origin flags
      controller->input.buttons |= JOYBUS_GCN_USE_ORIGIN;

      // Save the analog mode and motor state
      joybus_id_clear_status_flags(controller->id, JOYBUS_ID_GCN_MOTOR_STATE_MASK | JOYBUS_ID_GCN_ANALOG_MODE_MASK);
      joybus_id_set_status_flags(controller->id, motor_state << JOYBUS_ID_GCN_MOTOR_STATE_SHIFT | analog_mode);
    }

    // If motor state has changed, call the motor state change callback
    // TODO
  }

  return JOYBUS_CMD_GCN_READ_LONG_TX - bytes_read;
}

/**
 * Handle "probe device" commands.
 *
 * Probe device is exclusively used by "launch window" games, I'm assuming to detect
 * capabilities of wireless controllers that were never actually released. Later games
 * will just use the "info" command to detect if a controller is wireless.
 *
 * An OEM WaveBird receiver will respond to this command with 8 bytes of zeroes until
 * it has received packets from a controller, at which point it will ignore further
 * probe commands.
 *
 * Command:         {0x4D, 0x??, 0x??} - 2nd and 3rd bytes seem to differ every time
 * Response:        8 bytes of zeroes.
 */
static inline int handle_probe_device(struct joybus_gc_controller *controller, const uint8_t *command,
                                      uint8_t bytes_read, joybus_target_response_cb_t send_response, void *user_data)
{
  if (bytes_read == 1) {
    // Don't respond to probe commands if we already received data from a controller
    if (joybus_id_get_type(controller->id) & JOYBUS_ID_GCN_WIRELESS_RECEIVED)
      return 0;

    // Respond with 8 bytes of zeroes
    static const uint8_t zeroes[8] = {0};
    send_response(zeroes, JOYBUS_CMD_GCN_PROBE_DEVICE_RX, user_data);
  }

  return JOYBUS_CMD_GCN_PROBE_DEVICE_TX - bytes_read;
}

/**
 * Handle "fix device" commands, to "fix" the receiver ID to a specific controller ID.
 *
 * This is used to pair a WaveBird controller with a specific receiver.
 *
 * Command:         {0x4E, wireless_id_h | 0x10, wireless_id_l}
 * Response:        A 3-byte controller ID
 */
static inline int handle_fix_device(struct joybus_gc_controller *controller, const uint8_t *command, uint8_t bytes_read,
                                    joybus_target_response_cb_t send_response, void *user_data)
{
  if (bytes_read == JOYBUS_CMD_GCN_FIX_DEVICE_TX) {
    // Extract the wireless ID from the command
    uint16_t wireless_id = ((command[1] & 0xC0) << 2) | command[2];

    // Save the wireless ID
    joybus_id_set_wireless_id(controller->id, wireless_id);

    // Update other controller ID flags
    joybus_id_set_type_flags(controller->id,
                             JOYBUS_ID_GCN_STANDARD | JOYBUS_ID_GCN_WIRELESS_STATE | JOYBUS_ID_GCN_WIRELESS_ID_FIXED);

    // Respond with the new controller ID
    send_response(controller->id, JOYBUS_CMD_GCN_FIX_DEVICE_RX, user_data);
  }

  return JOYBUS_CMD_GCN_FIX_DEVICE_TX - bytes_read;
}

static int gc_controller_byte_received(struct joybus_target *target, const uint8_t *command, uint8_t bytes_read,
                                       joybus_target_response_cb_t send_response, void *user_data)
{
  struct joybus_gc_controller *controller = JOYBUS_GC_CONTROLLER(target);
  switch (command[0]) {
    case JOYBUS_CMD_RESET:
      return handle_reset(controller, command, bytes_read, send_response, user_data);
    case JOYBUS_CMD_IDENTIFY:
      return handle_identify(controller, command, bytes_read, send_response, user_data);
    case JOYBUS_CMD_GCN_READ:
      return handle_read(controller, command, bytes_read, send_response, user_data);
    case JOYBUS_CMD_GCN_READ_ORIGIN:
      return handle_read_origin(controller, command, bytes_read, send_response, user_data);
    case JOYBUS_CMD_GCN_CALIBRATE:
      return handle_calibrate(controller, command, bytes_read, send_response, user_data);
    case JOYBUS_CMD_GCN_READ_LONG:
      return handle_read_long(controller, command, bytes_read, send_response, user_data);
    case JOYBUS_CMD_GCN_PROBE_DEVICE:
      return handle_probe_device(controller, command, bytes_read, send_response, user_data);
    case JOYBUS_CMD_GCN_FIX_DEVICE:
      return handle_fix_device(controller, command, bytes_read, send_response, user_data);
  }

  return -JOYBUS_ERR_NOT_SUPPORTED;
}

static const struct joybus_target_api gc_controller_api = {
  .byte_received = gc_controller_byte_received,
};

void joybus_gc_controller_init(struct joybus_gc_controller *controller, uint16_t type)
{
  // Set the target callbacks
  struct joybus_target *target = JOYBUS_TARGET(controller);
  target->api                  = &gc_controller_api;

  // Set the initial controller ID
  memset(controller->id, 0, sizeof(controller->id));
  joybus_id_set_type_flags(controller->id, type);

  // Set the initial origin
  memset(&controller->origin, 0, sizeof(controller->origin));
  controller->origin.stick_x    = 0x80;
  controller->origin.stick_y    = 0x80;
  controller->origin.substick_x = 0x80;
  controller->origin.substick_y = 0x80;

  // Set the initial input state
  memcpy(&controller->input, &controller->origin, sizeof(controller->input));

  // Mark the input as valid initially
  controller->input_valid = true;
}

void joybus_gc_controller_set_wireless_id(struct joybus_gc_controller *controller, uint16_t wireless_id)
{
  if (joybus_gc_controller_wireless_id_fixed(controller))
    return;

  // Set the wireless ID
  joybus_id_set_wireless_id(controller->id, wireless_id);

  // Update other controller ID flags
  joybus_id_set_type_flags(controller->id, JOYBUS_ID_GCN_STANDARD | JOYBUS_ID_GCN_WIRELESS_RECEIVED);
}

void joybus_gc_controller_set_origin(struct joybus_gc_controller *controller,
                                     struct joybus_gc_controller_input *new_origin)
{
  // Check if the analog values in the new origin differ from the current origin
  if (memcmp(&controller->origin.stick_x, &new_origin->stick_x, 6) != 0) {
    // Update the origin state
    memcpy(&controller->origin.stick_x, &new_origin->stick_x, 6);

    // Tell the host that new origin data is available
    set_need_origin(controller, true);
  }

  // Set the "has wireless origin" flag in the device ID
  if (joybus_gc_controller_is_wireless(controller))
    joybus_id_set_type_flags(controller->id, JOYBUS_ID_GCN_WIRELESS_ORIGIN);
}
