#include <stddef.h>
#include <string.h>

#include <joybus/errors.h>
#include <joybus/common/n64_pak_fs.h>

// Offsets of the four ID copies in page 0
#define ID_COPIES 4
static const uint16_t id_offsets[ID_COPIES] = {0x20, 0x60, 0x80, 0xC0};

// ID block layout, 32 bytes with the checksums covering the first 28
#define ID_SIZE         32
#define ID_SUMMED_BYTES 28
#define ID_REPAIRED     0x00
#define ID_RANDOM       0x04
#define ID_DEVICE       0x18
#define ID_BANKS        0x1A
#define ID_VERSION      0x1B
#define ID_CHECKSUM     0x1C
#define ID_INV_CHECKSUM 0x1E

// Device ID with the low bit set, which marks a Controller Pak
#define ID_DEVICE_PAK 0x0001

// Inode entries are two bytes
#define INODE_ENTRIES (JOYBUS_N64_PAK_FS_PAGE_SIZE / 2)

// Inode entry of a free page
#define INODE_FREE 0x0003

// Pak fields are big endian
static uint16_t read16(const uint8_t *p)
{
  return (uint16_t)(p[0] << 8 | p[1]);
}

static void write16(uint8_t *p, uint16_t value)
{
  p[0] = value >> 8;
  p[1] = value & 0xFF;
}

static void write32(uint8_t *p, uint32_t value)
{
  write16(p, value >> 16);
  write16(p + 2, value & 0xFFFF);
}

// Compute the two ID checksums a console verifies
static void id_checksums(const uint8_t *id, uint16_t *sum, uint16_t *inverted)
{
  *sum      = 0;
  *inverted = 0;

  // Sum the 16-bit halves of the ID, and their complements
  for (size_t i = 0; i < ID_SUMMED_BYTES; i += 2) {
    uint16_t half = read16(&id[i]);
    *sum += half;
    *inverted += (uint16_t)~half;
  }
}

// Check whether one ID copy is intact and describes the given bank count
static bool id_valid(const uint8_t *id, uint8_t banks)
{
  // Both checksums must match
  uint16_t sum, inverted;
  id_checksums(id, &sum, &inverted);
  if (read16(&id[ID_CHECKSUM]) != sum || read16(&id[ID_INV_CHECKSUM]) != inverted)
    return false;

  // The low bit of the device ID marks a Controller Pak
  if ((read16(&id[ID_DEVICE]) & 1) == 0)
    return false;

  // The ID must name the bank count the pak presents
  return id[ID_BANKS] == banks;
}

// Build an inode page with every data page free
static void inode_page(uint8_t *page, uint8_t bank, uint8_t banks)
{
  // Bank 0's data starts after the system area, later banks' after a reserved first page
  size_t first = bank == 0 ? JOYBUS_N64_PAK_FS_SYSTEM_PAGES(banks) : 1;

  // Mark every data page free
  memset(page, 0, JOYBUS_N64_PAK_FS_PAGE_SIZE);
  for (size_t entry = first; entry < INODE_ENTRIES; entry++)
    write16(&page[entry * 2], INODE_FREE);

  // Byte 1 holds the byte sum of the entries from the first data page on
  uint8_t sum = 0;
  for (size_t i = first * 2; i < JOYBUS_N64_PAK_FS_PAGE_SIZE; i++)
    sum += page[i];
  page[1] = sum;
}

bool joybus_n64_pak_fs_valid(const uint8_t *bank0, uint8_t banks)
{
  // Any one intact copy is enough, a console repairs the rest from it
  for (int i = 0; i < ID_COPIES; i++) {
    if (id_valid(&bank0[id_offsets[i]], banks))
      return true;
  }

  return false;
}

int joybus_n64_pak_fs_format(uint8_t *bank0, uint8_t banks, uint32_t random)
{
  if (banks == 0 || banks > JOYBUS_N64_PAK_FS_MAX_BANKS)
    return -JOYBUS_ERR_INVALID_ARG;

  int system = JOYBUS_N64_PAK_FS_SYSTEM_PAGES(banks) * JOYBUS_N64_PAK_FS_PAGE_SIZE;

  // Clear the system area, which leaves the note table empty
  memset(bank0, 0, system);

  // Build the ID block as a console writes one
  uint8_t id[ID_SIZE] = {0};
  write32(&id[ID_REPAIRED], 0xFFFFFFFF);

  // A console compares the whole ID on every access, so the random word is what tells paks apart
  write32(&id[ID_RANDOM], random);
  write16(&id[ID_DEVICE], ID_DEVICE_PAK);
  id[ID_BANKS]   = banks;
  id[ID_VERSION] = 0;

  // Checksum the ID
  uint16_t sum, inverted;
  id_checksums(id, &sum, &inverted);
  write16(&id[ID_CHECKSUM], sum);
  write16(&id[ID_INV_CHECKSUM], inverted);

  // Write all four copies of the ID
  for (int i = 0; i < ID_COPIES; i++)
    memcpy(&bank0[id_offsets[i]], id, ID_SIZE);

  // Write one inode page per bank from page 1, then a mirror of each
  for (uint8_t bank = 0; bank < banks; bank++) {
    uint8_t *table  = &bank0[(1 + bank) * JOYBUS_N64_PAK_FS_PAGE_SIZE];
    uint8_t *mirror = &bank0[(1 + banks + bank) * JOYBUS_N64_PAK_FS_PAGE_SIZE];

    inode_page(table, bank, banks);
    memcpy(mirror, table, JOYBUS_N64_PAK_FS_PAGE_SIZE);
  }

  return system;
}
