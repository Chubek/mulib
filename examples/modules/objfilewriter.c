#define MULIB_IMPL_OBJFILEWRITER
#define MULIB_IMPL_BINFILEWRITER
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#include "mulib-objfilewriter.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t b = { 0 };
  mulib_objfilewriter_section_t s = { ".text", "\xc3", 1, 16, 6 };
  assert (mulib_objfilewriter_write (&b, 62, &s, 1) && b.data[0] == 127);
  mulib_buffer_free (&b);
  return 0;
}
