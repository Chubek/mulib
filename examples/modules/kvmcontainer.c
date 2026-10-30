#define MULIB_IMPL_KVMCONTAINER
#define MULIB_IMPL_KVM
#include "mulib-kvmcontainer.h"
#include <assert.h>
int
main (void)
{
  mulib_kvmcontainer_t c;
  mulib_kvmcontainer_init (&c);
  assert (
      !mulib_kvmcontainer_close (&c)); /* Hardware-free lifetime example. */
  return 0;
}
