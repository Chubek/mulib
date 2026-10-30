#define MULIB_IMPL_BLOCKCACHE
#include "mulib-blockcache.h"
#include <assert.h>
int
main (void)
{
  mulib_blockcache_t c;
  const void *data;
  size_t n;
  assert (mulib_blockcache_init (&c, 2, 1024));
  assert (mulib_blockcache_put (&c, 7, "block", 5));
  assert (mulib_blockcache_get (&c, 7, &data, &n) && n == 5);
  mulib_blockcache_free (&c);
  return 0;
}
