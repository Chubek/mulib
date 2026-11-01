#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_VMMDEV
#define MULIB_IMPL_VMM_MEMORY
#include "mulib-vmm-memory.h"
#include <assert.h>
#include <string.h>

int main(void)
{
  unsigned char first[16] = {0}, second[16] = {0}, copy[32];
  const unsigned char image[] = "guest image across two regions";
  mulib_vmmdev_region_t regions[] = {{0x1000,16,first,3},{0x1010,16,second,3}};
  mulib_vmmdev_memory_t memory = {regions, 2};
  assert(!mulib_vmm_memory_write(&memory, 0x1000, image, sizeof image));
  assert(!mulib_vmm_memory_read(&memory, 0x1000, copy, sizeof image));
  assert(!memcmp(copy, image, sizeof image));
  assert(!mulib_vmm_memory_zero(&memory, 0x1000, 32));
  return 0;
}
