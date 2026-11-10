#define MULIB_IMPL_ITERATOR
#include "mulib-iterator.h"
#include <assert.h>
static int
next (void *state, const void **out)
{
  int *n = state;
  if (*n == 3)
    return 0;
  ++*n;
  *out = n;
  return 1;
}
int
main (void)
{
  int n = 0, total = 0;
  const void *item;
  mulib_iterator_t it = { &n, next, NULL };
  while (mulib_iterator_next (&it, &item) == 1)
    total += *(const int *)item;
  assert (total == 6);
  mulib_iterator_close (&it);
  return 0;
}
