#define MULIB_IMPL_OCICONTAINER
#include "mulib-ocicontainer.h"
#include <assert.h>
int
main (void)
{
  mulib_ocicontainer_t c;
  assert (mulib_ocicontainer_create (&c, "demo"));
  assert (mulib_ocicontainer_transition (&c, MULIB_OCICONTAINER_RUNNING, 0));
  assert (mulib_ocicontainer_transition (&c, MULIB_OCICONTAINER_STOPPED, 0));
  return 0;
}
