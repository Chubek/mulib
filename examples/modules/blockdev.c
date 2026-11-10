#define MULIB_IMPL_BLOCKDEV
#include "mulib-blockdev.h"
#include <assert.h>
static int
read_block (void *ctx, uint64_t block, void *out, size_t size)
{
  if (block != 0)
    return 0;
  memcpy (out, ctx, size);
  return 1;
}
int
main (void)
{
  char storage[4] = { 'd', 'a', 't', 'a' }, out[4];
  mulib_blockdev_t device = { storage, 4, read_block, NULL, NULL };
  assert (mulib_blockdev_read (&device, 0, out) && !memcmp (out, storage, 4));
  assert (mulib_blockdev_flush (&device));
  return 0;
}
