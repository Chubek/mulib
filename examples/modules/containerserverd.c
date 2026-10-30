#define MULIB_IMPL_CONTAINERSERVERD
#define MULIB_IMPL_OCICONTAINERRUNTIME
#define MULIB_IMPL_OCICONTAINERMETADATA
#define MULIB_IMPL_OCICONTAINER
#define MULIB_IMPL_CONTAINER2DAEMONPROTO
#define MULIB_IMPL_CONTAINERSTUB
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-containerserverd.h"
#include "mulib-containerstub.h"
#include <assert.h>
static int
action (void *ctx, const mulib_ocicontainer_t *c, int next, int code)
{
  (void)ctx;
  (void)c;
  (void)next;
  (void)code;
  return 0;
}
int
main (void)
{
  mulib_ocicontainerruntime_t r;
  mulib_buffer_t request = { 0 }, response = { 0 };
  assert (mulib_ocicontainerruntime_init (&r, "demo", action, NULL));
  assert (mulib_containerstub_request (&request, 4, 1, "demo"));
  assert (!mulib_containerserverd_dispatch (&r, request.data, request.length,
                                            &response));
  mulib_buffer_free (&request);
  mulib_buffer_free (&response);
  return 0;
}
