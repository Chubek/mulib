#define MULIB_IMPL_OBJFILEBUILDER
#define MULIB_IMPL_OBJFILEWRITER
#define MULIB_IMPL_BINFILEWRITER
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#include "mulib-objfilebuilder.h"
#include <assert.h>
int
main (void)
{
  mulib_objfilebuilder_t builder = { 0 };
  mulib_buffer_t b = { 0 };
  assert (mulib_objfilebuilder_add (&builder, ".data", "abc", 3, 1, 3));
  assert (mulib_objfilebuilder_finish (&builder, 62, &b));
  mulib_objfilebuilder_free (&builder);
  mulib_buffer_free (&b);
  return 0;
}
