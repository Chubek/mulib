#define MULIB_IMPL_BINFILESCHEMA
#define MULIB_IMPL_BYCUTILS
#include "mulib-binfileschema.h"
#include <assert.h>
int
main (void)
{
  mulib_binfileschema_field_t field = { "count", 0, 2 };
  mulib_binfileschema_t s = { &field, 1, 2 };
  unsigned char bytes[] = { 42, 0 };
  uint64_t n;
  assert (mulib_binfileschema_get (&s, bytes, 2, "count", &n) && n == 42);
  return 0;
}
