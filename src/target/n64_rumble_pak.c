#include <string.h>

#include <joybus/bus.h>
#include <joybus/target/n64_pak.h>
#include <joybus/target/n64_rumble_pak.h>

#define RUMBLE_PAK_REGION_MASK  0xC000
#define RUMBLE_PAK_PROBE_REGION 0x8000
#define RUMBLE_PAK_MOTOR_REGION 0xC000
#define RUMBLE_PAK_SIGNATURE    0x80

static void rumble_pak_read_block(struct joybus_target_n64_pak *pak, uint16_t addr, uint8_t buf[JOYBUS_PAK_BLOCK_SIZE])
{
  struct joybus_target_n64_rumble_pak *rumble_pak = JOYBUS_TARGET_N64_RUMBLE_PAK(pak);

  // The entire probe region returns the signature while enabled; the rest
  // of the address space (SRAM space, motor region) reads as zeros.
  if ((addr & RUMBLE_PAK_REGION_MASK) == RUMBLE_PAK_PROBE_REGION && rumble_pak->enabled) {
    memset(buf, RUMBLE_PAK_SIGNATURE, JOYBUS_PAK_BLOCK_SIZE);
  } else {
    memset(buf, 0x00, JOYBUS_PAK_BLOCK_SIZE);
  }
}

static void rumble_pak_write_block(struct joybus_target_n64_pak *pak, uint16_t addr,
                                   const uint8_t buf[JOYBUS_PAK_BLOCK_SIZE])
{
  struct joybus_target_n64_rumble_pak *rumble_pak = JOYBUS_TARGET_N64_RUMBLE_PAK(pak);

  // Both the probe register and the motor latch the last byte of the write
  uint8_t last = buf[JOYBUS_PAK_BLOCK_SIZE - 1];

  // A probe-region write of exactly 0x80 sets the enable register
  if ((addr & RUMBLE_PAK_REGION_MASK) == RUMBLE_PAK_PROBE_REGION) {
    rumble_pak->enabled = (last == RUMBLE_PAK_SIGNATURE);
    return;
  }

  // Writes to SRAM address space are ignored
  if ((addr & RUMBLE_PAK_REGION_MASK) != RUMBLE_PAK_MOTOR_REGION)
    return;

  // The motor state is the low bit of the last byte, and only runs while enabled
  bool active = rumble_pak->enabled && (last & 1);
  if (active == rumble_pak->active)
    return;

  // Update cached motor state
  rumble_pak->active = active;

  // Fire callback if motor state has changed
  if (rumble_pak->on_motor_change)
    rumble_pak->on_motor_change(rumble_pak, active);
}

static const struct joybus_target_n64_pak_api rumble_pak_api = {
  .read_block  = rumble_pak_read_block,
  .write_block = rumble_pak_write_block,
};

void joybus_target_n64_rumble_pak_init(struct joybus_target_n64_rumble_pak *rumble_pak)
{
  // Start from a clean state
  memset(rumble_pak, 0, sizeof(*rumble_pak));

  // Set the base pak API implementation
  struct joybus_target_n64_pak *pak = JOYBUS_TARGET_N64_PAK(rumble_pak);
  pak->api = &rumble_pak_api;
}

void joybus_target_n64_rumble_pak_set_motor_cb(struct joybus_target_n64_rumble_pak *rumble_pak,
                                               joybus_target_n64_rumble_pak_motor_cb_t callback)
{
  rumble_pak->on_motor_change = callback;
}
