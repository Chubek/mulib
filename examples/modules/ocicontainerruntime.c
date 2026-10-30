#define MULIB_IMPL_OCICONTAINERRUNTIME
#define MULIB_IMPL_OCICONTAINER
#include "mulib-ocicontainerruntime.h"
#include <assert.h>
/* Test backend only: no OS process or isolation is created. */
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
  assert (mulib_ocicontainerruntime_init (&r, "demo", action, NULL));
  assert (
      !mulib_ocicontainerruntime_apply (&r, MULIB_OCICONTAINER_RUNNING, 0));
  return 0;
}
