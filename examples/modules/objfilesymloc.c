#define MULIB_IMPL_OBJFILESYMLOC
#define MULIB_IMPL_BINFILE
#define MULIB_IMPL_ELFUTILS
#define MULIB_IMPL_ELF
#define MULIB_IMPL_ENDIAN
#include "mulib-objfilesymloc.h"
#include <assert.h>
int
main (void)
{
  /* Minimal empty ET_REL ELF64: no symbols, no sections. */
  unsigned char bytes[64] = { 0x7f, 'E', 'L', 'F', 2, 1, 1 };
  mulib_elf_t elf;
  mulib_binfile_t symbol;
  bytes[16] = 1;
  bytes[18] = 62;
  bytes[20] = 1;
  bytes[52] = 64;
  assert (!mulib_elf_open (&elf, bytes, 64));
  assert (mulib_objfilesymloc_find (&elf, "absent", &symbol)
          == MULIB_ELF_ENOTFOUND);
  return 0;
}
