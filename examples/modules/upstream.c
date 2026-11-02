#define MULIB_IMPL_UPSTREAM
#include "mulib-upstream.h"
#include <assert.h>
int
main (void)
{
  mulib_upstream_t server = { "localhost", 8080, 0, 1, 1 };
  assert (mulib_upstream_acquire (&server));
  assert (!mulib_upstream_acquire (&server));
  assert (mulib_upstream_release (&server));
  return 0;
}
