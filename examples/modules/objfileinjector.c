#define MULIB_IMPL_OBJFILEINJECTOR
#define MULIB_IMPL_ELFUTILS
#define MULIB_IMPL_ELF
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_OBJFILEWRITER
#define MULIB_IMPL_BINFILEWRITER
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#include "mulib-objfileinjector.h"
#include "mulib-objfilewriter.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t b = { 0 };
  mulib_objfilewriter_section_t s = { ".data", "abc", 3, 1, 3 };
  assert (mulib_objfilewriter_write (&b, 62, &s, 1));
  assert (
      !mulib_objfileinjector_replace (b.data, b.length, ".data", "xyz", 3));
  mulib_buffer_free (&b);
  return 0;
}
