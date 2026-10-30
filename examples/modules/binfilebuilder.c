#define MULIB_IMPL_BINFILEBUILDER
#define MULIB_IMPL_BINFILESCHEMA
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_BUFFER
#include "mulib-binfilebuilder.h"
#include <assert.h>
int
main (void)
{
  mulib_binfileschema_field_t field = { "count", 0, 2 };
  mulib_binfileschema_t s = { &field, 1, 2 };
  mulib_buffer_t b = { 0 };
  uint64_t value = 42;
  assert (mulib_binfilebuilder_record (&b, &s, &value, 1) && b.length == 2
          && b.data[0] == 42);
  mulib_buffer_free (&b);
  return 0;
}
