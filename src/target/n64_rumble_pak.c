#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <joybus/bus.h>
#include <joybus/target/n64_rumble_pak.h>

// Rumble pak probe region: reads return 0x80 x 32 as the rumble pak signature
#define RUMBLE_PAK_PROBE_BASE 0x8000
#define RUMBLE_PAK_PROBE_END  0xA000

// Rumble pak motor control region: writes toggle the motor based on buf[0]
#define RUMBLE_PAK_MOTOR_BASE 0xC000
#define RUMBLE_PAK_MOTOR_END  0xE000

static inline bool addr_in_range(uint16_t addr, uint16_t base, uint16_t end)
{
  return addr >= base && addr < end;
}

static void rumble_pak_read_block(struct joybus_target_n64_pak *base, uint16_t addr, uint8_t buf[JOYBUS_PAK_BLOCK_SIZE])
{
  (void)base;

  // The probe region returns the rumble pak signature (0x80 x 32),
  // everything else returns zeros.
  if (addr_in_range(addr, RUMBLE_PAK_PROBE_BASE, RUMBLE_PAK_PROBE_END)) {
    memset(buf, 0x80, JOYBUS_PAK_BLOCK_SIZE);
  } else {
    memset(buf, 0x00, JOYBUS_PAK_BLOCK_SIZE);
  }
}

static void rumble_pak_write_block(struct joybus_target_n64_pak *base, uint16_t addr,
                                   const uint8_t buf[JOYBUS_PAK_BLOCK_SIZE])
{
  struct joybus_target_n64_rumble_pak *pak = JOYBUS_TARGET_N64_RUMBLE_PAK(base);

  // Writes outside the motor control region are ignored
  if (!addr_in_range(addr, RUMBLE_PAK_MOTOR_BASE, RUMBLE_PAK_MOTOR_END)) {
    return;
  }

  // The first byte of the payload determines the motor state
  bool active = buf[0] != 0;
  if (active == pak->active) {
    return;
  }

  pak->active = active;
  if (pak->on_motor_change) {
    pak->on_motor_change(pak, active);
  }
}

static const struct joybus_target_n64_pak_api rumble_pak_api = {
  .read_block  = rumble_pak_read_block,
  .write_block = rumble_pak_write_block,
};

void joybus_target_n64_rumble_pak_init(struct joybus_target_n64_rumble_pak *pak)
{
  memset(pak, 0, sizeof(*pak));
  pak->base.api = &rumble_pak_api;
}

void joybus_target_n64_rumble_pak_set_motor_cb(struct joybus_target_n64_rumble_pak *pak,
                                               joybus_target_n64_rumble_pak_motor_cb_t callback)
{
  pak->on_motor_change = callback;
}
