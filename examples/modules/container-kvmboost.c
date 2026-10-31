#include "../container-native-common.h"
#include <stdio.h>
int main (void)
{
  int result = mulib_container_kvmboost_probe (NULL);
  printf ("KVM probe: %s\n", mulib_ct_strerror (result));
  /* A usable backend supplies guest configure/reset/exit callbacks; the probe
   * opens and closes only the device and makes no guest or host mounts. */
  assert (mulib_container_kvmboost_probe ("/mulib-missing-kvm") < 0);
  return 0;
}
