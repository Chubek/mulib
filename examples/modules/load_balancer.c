#define MULIB_IMPL_LOAD_BALANCER
#define MULIB_IMPL_UPSTREAM
#include "mulib-load_balancer.h"
#include <assert.h>
int
main (void)
{
  mulib_upstream_t servers[] = { { "a", 80, 0, 10, 1 }, { "b", 80, 0, 10, 1 } };
  size_t cursor = 0,
         selected = mulib_load_balancer_select (servers, 2, &cursor, 0);
  assert (selected == 0 && mulib_upstream_acquire (servers + selected));
  assert (mulib_load_balancer_select (servers, 2, &cursor, 1) == 1);
  assert (mulib_upstream_release (servers + selected));
  return 0;
}
