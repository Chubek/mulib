#define MULIB_IMPL_ABSTUN
#define MULIB_IMPL_FILECTL
#define MULIB_IMPL_TUNTAP
#include "mulib-tuntap.h"
#include <assert.h>
int
main (void)
{
  mulib_tuntap_t tun = MULIB_TUNTAP_INIT;
  mulib_abstun_t packets = mulib_tuntap_backend (&tun);
  size_t n;
  char data[1500];
  /* Unprivileged example: validate lifecycle without creating an interface.
     A privileged application can open(&tun, "", 0, 1500), configure the
     returned interface externally, then use the abstract packet backend. */
  assert (mulib_tuntap_open (&tun, "name-that-is-too-long", 0, 1500) == -1);
  assert (mulib_abstun_receive (&packets, data, sizeof data, &n) == -1);
#ifdef __linux__
  assert (!mulib_tuntap_close (&tun));
#endif
  return 0;
}
