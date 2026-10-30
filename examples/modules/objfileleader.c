#define MULIB_IMPL_OBJFILELEADER
#define MULIB_IMPL_ELF
#define MULIB_IMPL_ENDIAN
#include "mulib-objfileleader.h"
#include <assert.h>
int
main (void)
{
  /* Empty ET_EXEC image has no segments to copy and leaves gaps unchanged. */
  unsigned char bytes[64] = { 0x7f, 'E', 'L', 'F', 2, 1, 1 },
                memory[8] = { 42 };
  mulib_elf_t elf;
  bytes[16] = 2;
  bytes[18] = 62;
  bytes[20] = 1;
  bytes[52] = 64;
  assert (!mulib_elf_open (&elf, bytes, 64));
  assert (!mulib_objfileleader_load (&elf, 0, memory, 8) && memory[0] == 42);
  return 0;
}
