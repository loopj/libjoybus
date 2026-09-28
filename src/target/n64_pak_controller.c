#include <string.h>

#include <joybus/attributes.h>
#include <joybus/bus.h>
#include <joybus/errors.h>
#include <joybus/common/n64_pak.h>
#include <joybus/target/n64_pak.h>
#include <joybus/target/n64_pak_controller.h>

// Address bits an original pak decodes, the rest alias onto the bank
#define PAK_CONTROLLER_BANK_MASK 0x7FFF

JOYBUS_RAM_FUNC
static int pak_controller_read_block(struct joybus_target_n64_pak *pak, uint16_t addr,
                                     uint8_t buf[JOYBUS_N64_PAK_BLOCK_SIZE])
{
  struct joybus_target_n64_pak_controller *pak_controller = JOYBUS_TARGET_N64_PAK_CONTROLLER(pak);

  // No storage yet reads as a pak that is not ready
  if (pak_controller->read == NULL)
    return -JOYBUS_ERR_BUSY;

  if (pak_controller->banks == 1) {
    // The probe area aliases onto the bank, which accessory detection relies on
    addr &= PAK_CONTROLLER_BANK_MASK;
  } else if (addr >= JOYBUS_N64_PAK_PROBE_ADDR) {
    // A banked pak reads zeros above the bank
    memset(buf, 0x00, JOYBUS_N64_PAK_BLOCK_SIZE);
    return 0;
  }

  return pak_controller->read(pak_controller, pak_controller->selected, addr, buf);
}

JOYBUS_RAM_FUNC
static int pak_controller_write_block(struct joybus_target_n64_pak *pak, uint16_t addr,
                                      const uint8_t buf[JOYBUS_N64_PAK_BLOCK_SIZE])
{
  struct joybus_target_n64_pak_controller *pak_controller = JOYBUS_TARGET_N64_PAK_CONTROLLER(pak);

  // No storage yet reads as a pak that is not ready
  if (pak_controller->write == NULL)
    return -JOYBUS_ERR_BUSY;

  if (pak_controller->banks == 1) {
    // The probe area aliases onto the bank
    addr &= PAK_CONTROLLER_BANK_MASK;
  } else if (addr >= JOYBUS_N64_PAK_PROBE_ADDR) {
    // A write to the probe area selects a bank, latched from the last byte
    if (addr < JOYBUS_N64_PAK_MOTOR_ADDR) {
      uint8_t requested = buf[JOYBUS_N64_PAK_BLOCK_SIZE - 1];

      // A select past the last bank is ignored, so an accessory type probe moves nothing
      if (requested < pak_controller->banks) {
        pak_controller->selected = requested;

        if (pak_controller->on_select)
          pak_controller->on_select(pak_controller, requested);
      }
    }

    return 0;
  }

  int result = pak_controller->write(pak_controller, pak_controller->selected, addr, buf);
  if (result != 0)
    return result;

  // Fire callback now the block is in storage
  if (pak_controller->on_written)
    pak_controller->on_written(pak_controller, pak_controller->selected, addr);

  return 0;
}

// Storage over a buffer of whole banks, laid out back to back
JOYBUS_RAM_FUNC
static int memory_read_block(struct joybus_target_n64_pak_controller *pak_controller, uint8_t bank, uint16_t addr,
                             uint8_t buf[JOYBUS_N64_PAK_BLOCK_SIZE])
{
  const uint8_t *memory = pak_controller->user_data;
  memcpy(buf, &memory[bank * JOYBUS_N64_PAK_BANK_SIZE + addr], JOYBUS_N64_PAK_BLOCK_SIZE);
  return 0;
}

JOYBUS_RAM_FUNC
static int memory_write_block(struct joybus_target_n64_pak_controller *pak_controller, uint8_t bank, uint16_t addr,
                              const uint8_t buf[JOYBUS_N64_PAK_BLOCK_SIZE])
{
  uint8_t *memory = pak_controller->user_data;
  memcpy(&memory[bank * JOYBUS_N64_PAK_BANK_SIZE + addr], buf, JOYBUS_N64_PAK_BLOCK_SIZE);
  return 0;
}

static JOYBUS_RAM_DATA const struct joybus_target_n64_pak_api pak_controller_api = {
  .read_block  = pak_controller_read_block,
  .write_block = pak_controller_write_block,
};

void joybus_target_n64_pak_controller_init(struct joybus_target_n64_pak_controller *pak_controller)
{
  // Start from a clean state
  memset(pak_controller, 0, sizeof(*pak_controller));

  // Set the base pak API implementation
  struct joybus_target_n64_pak *pak = JOYBUS_TARGET_N64_PAK(pak_controller);
  pak->api                          = &pak_controller_api;
}

void joybus_target_n64_pak_controller_set_storage(struct joybus_target_n64_pak_controller *pak_controller,
                                                  uint8_t banks, joybus_target_n64_pak_controller_read_cb read,
                                                  joybus_target_n64_pak_controller_write_cb write, void *user_data)
{
  pak_controller->read      = read;
  pak_controller->write     = write;
  pak_controller->user_data = user_data;

  // Take the shape of the new storage, starting from its first bank
  pak_controller->banks    = banks;
  pak_controller->selected = 0;
}

void joybus_target_n64_pak_controller_set_memory(struct joybus_target_n64_pak_controller *pak_controller,
                                                 uint8_t *memory, uint8_t banks)
{
  joybus_target_n64_pak_controller_set_storage(pak_controller, banks, memory_read_block, memory_write_block, memory);
}

void joybus_target_n64_pak_controller_set_select_cb(struct joybus_target_n64_pak_controller *pak_controller,
                                                    joybus_target_n64_pak_controller_select_cb callback)
{
  pak_controller->on_select = callback;
}

void joybus_target_n64_pak_controller_set_written_cb(struct joybus_target_n64_pak_controller *pak_controller,
                                                     joybus_target_n64_pak_controller_written_cb callback)
{
  pak_controller->on_written = callback;
}
