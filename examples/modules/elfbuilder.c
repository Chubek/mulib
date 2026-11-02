#define MULIB_IMPL_ELFBUILDER
#define MULIB_IMPL_OBJFILEBUILDER
#define MULIB_IMPL_OBJFILEWRITER
#define MULIB_IMPL_BINFILEWRITER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_BUFFER
#include "mulib-elfbuilder.h"
#include <assert.h>
int
main (void)
{
  mulib_elfbuilder_t builder = { 0 };
  mulib_buffer_t object = { 0 };
  const unsigned char ret[] = { 0xc3 };
  assert (mulib_elfbuilder_add (&builder, ".text", ret, 1, 16, 6));
  assert (mulib_elfbuilder_finish (&builder, 62, &object));
  assert (object.length > 64 && !memcmp (object.data, "\177ELF", 4));
  mulib_elfbuilder_free (&builder);
  mulib_buffer_free (&object);
  return 0;
}
