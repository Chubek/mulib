#define MULIB_IMPL_OCIIMAGE
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-ociimage.h"
#include <assert.h>
int
main (void)
{
  assert (mulib_ociimage_digest (
      "sha256:"
      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
  assert (!mulib_ociimage_parse ("{}", 2, NULL));
  return 0;
}
