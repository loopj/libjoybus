#include <string.h>

#include <joybus/attributes.h>
#include <joybus/bus.h>
#include <joybus/common/n64_pak.h>
#include <joybus/target/n64_pak.h>
#include <joybus/target/n64_pak_rumble.h>

#define PAK_RUMBLE_REGION_MASK 0xC000
#define PAK_RUMBLE_SIGNATURE   0x80

JOYBUS_RAM_FUNC
static int pak_rumble_read_block(struct joybus_target_n64_pak *pak, uint16_t addr, uint8_t buf[JOYBUS_N64_PAK_BLOCK_SIZE])
{
  struct joybus_target_n64_pak_rumble *pak_rumble = JOYBUS_TARGET_N64_PAK_RUMBLE(pak);

  // The entire probe region returns the signature while enabled; the rest
  // of the address space (SRAM space, motor region) reads as zeros.
  if ((addr & PAK_RUMBLE_REGION_MASK) == JOYBUS_N64_PAK_PROBE_ADDR && pak_rumble->enabled) {
    memset(buf, PAK_RUMBLE_SIGNATURE, JOYBUS_N64_PAK_BLOCK_SIZE);
  } else {
    memset(buf, 0x00, JOYBUS_N64_PAK_BLOCK_SIZE);
  }

  return 0;
}

JOYBUS_RAM_FUNC
static int pak_rumble_write_block(struct joybus_target_n64_pak *pak, uint16_t addr,
                                  const uint8_t buf[JOYBUS_N64_PAK_BLOCK_SIZE])
{
  struct joybus_target_n64_pak_rumble *pak_rumble = JOYBUS_TARGET_N64_PAK_RUMBLE(pak);

  // Both the probe register and the motor latch the last byte of the write
  uint8_t last = buf[JOYBUS_N64_PAK_BLOCK_SIZE - 1];

  // A probe-region write of exactly 0x80 sets the enable register
  if ((addr & PAK_RUMBLE_REGION_MASK) == JOYBUS_N64_PAK_PROBE_ADDR) {
    pak_rumble->enabled = (last == PAK_RUMBLE_SIGNATURE);
    return 0;
  }

  // Writes to SRAM address space are ignored
  if ((addr & PAK_RUMBLE_REGION_MASK) != JOYBUS_N64_PAK_MOTOR_ADDR)
    return 0;

  // The motor state is the low bit of the last byte, and only runs while enabled
  bool active = pak_rumble->enabled && (last & 1);
  if (active == pak_rumble->active)
    return 0;

  // Update cached motor state
  pak_rumble->active = active;

  // Fire callback if motor state has changed
  if (pak_rumble->on_motor_change)
    pak_rumble->on_motor_change(pak_rumble, active);

  return 0;
}

static JOYBUS_RAM_DATA const struct joybus_target_n64_pak_api pak_rumble_api = {
  .read_block  = pak_rumble_read_block,
  .write_block = pak_rumble_write_block,
};

void joybus_target_n64_pak_rumble_init(struct joybus_target_n64_pak_rumble *pak_rumble)
{
  // Start from a clean state
  memset(pak_rumble, 0, sizeof(*pak_rumble));

  // Set the base pak API implementation
  struct joybus_target_n64_pak *pak = JOYBUS_TARGET_N64_PAK(pak_rumble);
  pak->api                          = &pak_rumble_api;
}

void joybus_target_n64_pak_rumble_set_motor_cb(struct joybus_target_n64_pak_rumble *pak_rumble,
                                               joybus_target_n64_pak_rumble_motor_cb callback)
{
  pak_rumble->on_motor_change = callback;
}
