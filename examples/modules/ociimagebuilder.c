#define MULIB_IMPL_OCIIMAGEBUILDER
#define MULIB_IMPL_OCIIMAGE
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-ociimagebuilder.h"
#include <assert.h>
int
main (void)
{
  const char *digest
      = "sha256:"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  mulib_json_t *d = mulib_ociimagebuilder_descriptor (
      "application/vnd.oci.image.config.v1+json", digest, 2);
  mulib_json_t *m = mulib_ociimagebuilder_create (d);
  assert (m && mulib_ociimage_validate (m));
  mulib_json_destroy (d);
  mulib_json_destroy (m);
  return 0;
}
