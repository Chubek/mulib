#define MULIB_IMPL_OCICONTAINERNETWRORK
#define MULIB_IMPL_OCICONTAINERNETWORK
#define MULIB_IMPL_INET4
#define MULIB_IMPL_PARSEC
#include "mulib-ocicontainernetwrork.h"
#include <assert.h>
int
main (void)
{
  mulib_ocicontainernetwrork_t network;
  assert (mulib_ocicontainernetwrork_init (&network, "10.0.0.2", 24, NULL));
  return 0;
}
