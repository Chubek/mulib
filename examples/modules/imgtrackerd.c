#define MULIB_IMPL_IMGTRACKERD
#define MULIB_IMPL_MEMTABLE
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_OCIIMAGE
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-imgtrackerd.h"
#include <assert.h>
int
main (void)
{
  mulib_imgtrackerd_t t = { 0 };
  const char *d
      = "sha256:"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  assert (!mulib_imgtrackerd_retain (&t, d)
          && mulib_imgtrackerd_count (&t, d) == 1);
  assert (!mulib_imgtrackerd_release (&t, d));
  mulib_imgtrackerd_free (&t);
  return 0;
}
