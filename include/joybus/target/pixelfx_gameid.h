/**
 * @defgroup joybus_target_pixelfx_gameid PixelFX Game ID Target
 * @ingroup joybus_target
 *
 * Joybus target which listens for the PixelFX game ID command. N64 flash carts
 * and Swiss on the GameCube send it over the controller port so that attached
 * devices know which game is running. The command carries 10 bytes of game ID
 * and expects no reply, so this target never sends one.
 *
 * The ID bytes depend on the console. An N64 sends CRC1 and CRC2 from the ROM
 * header, the media format and the country code. A GameCube sends the 8-byte
 * boot hash, the console ID and the country code. All zeros clears the game ID.
 *
 * An N64 sends the command to the first controller port only. Swiss sends it to every port.
 *
 * @{
 */

#pragma once

#include <stdint.h>

#include <joybus/target.h>

struct joybus_target_pixelfx_gameid;

/// Macro to cast from a generic Joybus target to a PixelFX game ID target
#define JOYBUS_TARGET_PIXELFX_GAMEID(target) ((struct joybus_target_pixelfx_gameid *)(target))

/// Size of a PixelFX game ID, in bytes
#define JOYBUS_PIXELFX_GAMEID_SIZE 10

/**
 * Callback type for game ID received events.
 *
 * Runs in interrupt context, so it must return quickly and must not block.
 * Copy or flag the ID and act on it from the main loop.
 *
 * @param target the target that received the game ID, with the ID in its game_id field
 */
typedef void (*joybus_target_pixelfx_gameid_received_cb)(struct joybus_target_pixelfx_gameid *target);

/**
 * PixelFX game ID Joybus target.
 */
struct joybus_target_pixelfx_gameid {
  /// Base target interface
  struct joybus_target base;

  /// The last game ID received, all zeros until one arrives
  uint8_t game_id[JOYBUS_PIXELFX_GAMEID_SIZE];

  /// Callback for game ID received events
  joybus_target_pixelfx_gameid_received_cb on_received;
};

/**
 * Initialize a PixelFX game ID target.
 *
 * @param target the target to initialize
 */
void joybus_target_pixelfx_gameid_init(struct joybus_target_pixelfx_gameid *target);

/**
 * Set the game ID received callback for the target.
 *
 * @param target   the target to set the callback for
 * @param callback the callback function
 */
void joybus_target_pixelfx_gameid_set_received_cb(struct joybus_target_pixelfx_gameid *target,
                                                  joybus_target_pixelfx_gameid_received_cb callback);

/** @} */
