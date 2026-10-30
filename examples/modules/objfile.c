#define MULIB_IMPL_OBJFILE
#define MULIB_IMPL_BINFILE
#define MULIB_IMPL_ELFUTILS
#define MULIB_IMPL_ELF
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_OBJFILEWRITER
#define MULIB_IMPL_BINFILEWRITER
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#include "mulib-objfile.h"
#include "mulib-objfilewriter.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t b = { 0 };
  mulib_objfile_t obj;
  mulib_binfile_t bytes;
  mulib_objfilewriter_section_t s = { ".data", "abc", 3, 1, 3 };
  assert (mulib_objfilewriter_write (&b, 62, &s, 1)
          && !mulib_objfile_open (&obj, b.data, b.length));
  assert (!mulib_objfile_section (&obj, ".data", &bytes) && bytes.size == 3);
  mulib_buffer_free (&b);
  return 0;
}
