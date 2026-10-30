#define MULIB_IMPL_XTKS
#include "mulib-xtks.h"
#include <assert.h>
#include <string.h>
static int
write_chunk (void *ctx, const void *p, size_t n, size_t *count)
{
  size_t *total = ctx;
  (void)p;
  *count = n > 2 ? 2 : n;
  *total += *count;
  return 0;
}
int
main (void)
{
  size_t total = 0, offset = 0;
  mulib_xtks_t stream = { &total, NULL, write_chunk };
  assert (mulib_xtks_write_all (&stream, "hello", 5, &offset) == 0);
  assert (total == 5 && offset == 5);
  return 0;
}
