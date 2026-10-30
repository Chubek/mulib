#define MULIB_IMPL_GIT
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_ADLER32
#define MULIB_IMPL_VCSPACK
#include "mulib-vcspack.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static int
visit (void *ctx, const mulib_vcspack_object_t *o)
{
  ++*(int *)ctx;
  assert (o->type == 3 && o->length == 5 && !memcmp (o->data, "hello", 5));
  return 0;
}
int
main (void)
{
  mulib_vcspack_object_t o = { 3, "hello", 5 };
  unsigned char *pack, scratch[32];
  size_t n;
  int count = 0;
  assert (mulib_vcspack_encode (&o, 1, &pack, &n));
  assert (
      mulib_vcspack_decode (pack, n, scratch, sizeof scratch, visit, &count)
          == 1
      && count == 1);
  free (pack);
  return 0;
}
