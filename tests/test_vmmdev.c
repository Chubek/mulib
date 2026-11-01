#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_VMMDEV
#include "mulib-vmmdev.h"
#include <assert.h>
#include <string.h>

int main(void)
{
  unsigned char wire[128], ram[128], chain[32], copy[16];
  mulib_vmmdev_message_t m = {0}, decoded;
  mulib_vmmdev_region_t region = {0x1000, sizeof ram, ram, 3};
  mulib_vmmdev_memory_t memory = {&region, 1};
  mulib_vmmdev_chain_t c;
  unsigned char *mapped = NULL;
  size_t n;
  memset(ram, 0, sizeof ram);
  m.opcode = MULIB_VMMDEV_SUBMIT; m.device = 7; m.epoch = 3;
  m.token = UINT64_C(0x123456789abcdef0); m.argument = 2;
  m.count = 1; m.length = 3; m.payload = (const unsigned char *)"abc";
  memset(wire, 0xaa, sizeof wire); n = 42;
  assert(mulib_vmmdev_encode(&m, wire, &n) == MULIB_VMMDEV_ENOSPC && n == 43 && wire[0] == 0xaa);
  n = sizeof wire;
  assert(!mulib_vmmdev_encode(&m, wire, &n) && n == 43);
  assert(!memcmp(wire, "MVMD\1\0\7\0\7\0\0\0\3\0\0\0", 16));
  assert(wire[16] == 0xf0 && wire[23] == 0x12 && wire[32] == 1 && wire[36] == 3);
  assert(!mulib_vmmdev_decode(wire, n, &decoded));
  assert(decoded.token == m.token && decoded.length == 3 && !memcmp(decoded.payload, "abc", 3));
  for (size_t i = 0; i < n; ++i) assert(mulib_vmmdev_decode(wire, i, &decoded));
  assert(mulib_vmmdev_decode(wire, n + 1, &decoded));
  wire[4] = 2; assert(mulib_vmmdev_decode(wire, n, &decoded) == MULIB_VMMDEV_EVERSION); wire[4] = 1;
  assert(!mulib_vmmdev_memory_validate(&memory));
  assert(!mulib_vmmdev_map(&memory, 0x107f, 1, 2, &mapped) && mapped == ram + 127);
  assert(mulib_vmmdev_map(&memory, 0x107f, 2, 2, &mapped) == MULIB_VMMDEV_EBOUNDS);
  assert(mulib_vmmdev_map(&memory, UINT64_MAX, 2, 2, &mapped));
  region.permissions = 1;
  assert(mulib_vmmdev_map(&memory, 0x1000, 1, 2, &mapped) == MULIB_VMMDEV_EACCESS);
  region.permissions = 3;
  mulib_vmmdev_store64(chain, 0x1000); mulib_endian_store_le32(chain + 8, 8); mulib_endian_store_le32(chain + 12, 0);
  mulib_vmmdev_store64(chain + 16, 0x1010); mulib_endian_store_le32(chain + 24, 8); mulib_endian_store_le32(chain + 28, 1);
  memcpy(ram, "12345678", 8);
  assert(!mulib_vmmdev_chain_decode(&memory, chain, 32, 2, 64, &c));
  assert(!mulib_vmmdev_chain_read(&c, 2, copy, 4) && !memcmp(copy, "3456", 4));
  assert(!mulib_vmmdev_chain_write(&c, 2, "abcd", 4) && !memcmp(ram + 18, "abcd", 4));
  assert(mulib_vmmdev_chain_write(&c, 6, "xxxx", 4) == MULIB_VMMDEV_EBOUNDS && ram[22] == 0);
  mulib_vmmdev_store64(chain + 16, 0x1007);
  assert(mulib_vmmdev_chain_decode(&memory, chain, 32, 2, 64, &c) == MULIB_VMMDEV_EINVAL);
  mulib_vmmdev_store64(chain + 16, 0x1010); mulib_endian_store_le32(chain + 12, 1);
  mulib_endian_store_le32(chain + 28, 0);
  assert(mulib_vmmdev_chain_decode(&memory, chain, 32, 2, 64, &c) == MULIB_VMMDEV_EINVAL);
  mulib_endian_store_le32(chain + 12, 0); mulib_endian_store_le32(chain + 28, 1);
  assert(mulib_vmmdev_chain_decode(&memory, chain, 32, 2, 7, &c) == MULIB_VMMDEV_EBOUNDS);
  /* Host aliases and guest aliases both invalidate a region inventory. */
  {
    mulib_vmmdev_region_t regions[2] = {region, region};
    mulib_vmmdev_memory_t aliases = {regions, 2};
    regions[1].address += 1024;
    assert(mulib_vmmdev_memory_validate(&aliases) == MULIB_VMMDEV_EINVAL);
  }
  /* Deterministic malformed descriptor mutation; decoding may never write RAM. */
  for (unsigned i = 0; i < 256; ++i) {
    unsigned char saved = chain[i % 32];
    chain[i % 32] = (unsigned char)i;
    (void)mulib_vmmdev_chain_decode(&memory, chain, 32, 2, 64, &c);
    assert(!memcmp(ram, "12345678", 8)); chain[i % 32] = saved;
  }
  return 0;
}
