#define MULIB_IMPL_FCA
#define MULIB_IMPL_NEXTCLOSURE
#include "mulib-nextclosure.h"
#include <assert.h>
int
main (void)
{
  mulib_fca_t c;
  assert (mulib_fca_init (&c, 2, 2));
  assert (mulib_fca_set (&c, 0, 0, 1) && mulib_fca_set (&c, 1, 1, 1));
  uint64_t intent = mulib_nextclosure_first (&c);
  size_t count = 1;
  while (mulib_nextclosure_next (&c, intent, &intent))
    count++;
  assert (count == 4 && intent == 3);
  return 0;
}
