#define MULIB_IMPL_OCICONTAINERMETADATA
#define MULIB_IMPL_OCICONTAINER
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-ocicontainermetadata.h"
#include <assert.h>
int
main (void)
{
  mulib_ocicontainer_t c;
  mulib_json_t *state;
  assert (mulib_ocicontainer_create (&c, "demo"));
  state = mulib_ocicontainermetadata_json (&c);
  assert (state && mulib_json_get (state, "status"));
  mulib_json_destroy (state);
  return 0;
}
