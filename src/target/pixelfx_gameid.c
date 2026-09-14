#include <string.h>

#include <joybus/attributes.h>
#include <joybus/commands.h>
#include <joybus/errors.h>
#include <joybus/target.h>
#include <joybus/target/pixelfx_gameid.h>

JOYBUS_RAM_FUNC
static int pixelfx_gameid_byte_received(struct joybus_target *target, const uint8_t *command, uint8_t bytes_read,
                                        joybus_target_response_cb send_response, void *user_data)
{
  struct joybus_target_pixelfx_gameid *gameid = JOYBUS_TARGET_PIXELFX_GAMEID(target);

  // Decline every other command
  if (command[0] != JOYBUS_CMD_PIXELFX_GAMEID)
    return -JOYBUS_ERR_NOT_SUPPORTED;

  // Wait until the full command is received
  if (bytes_read < JOYBUS_CMD_PIXELFX_GAMEID_TX)
    return JOYBUS_CMD_PIXELFX_GAMEID_TX - bytes_read;

  // Store the game ID and notify, the sender expects no reply
  memcpy(gameid->game_id, &command[1], JOYBUS_PIXELFX_GAMEID_SIZE);
  if (gameid->on_received)
    gameid->on_received(gameid);

  return 0;
}

static const struct joybus_target_api pixelfx_gameid_api = {
  .byte_received = pixelfx_gameid_byte_received,
};

void joybus_target_pixelfx_gameid_init(struct joybus_target_pixelfx_gameid *target)
{
  // Start from a clean state
  memset(target, 0, sizeof(*target));

  // Set the base target API implementation
  target->base.api = &pixelfx_gameid_api;
}

void joybus_target_pixelfx_gameid_set_received_cb(struct joybus_target_pixelfx_gameid *target,
                                                  joybus_target_pixelfx_gameid_received_cb callback)
{
  target->on_received = callback;
}
