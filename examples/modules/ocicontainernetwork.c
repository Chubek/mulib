#define MULIB_IMPL_OCICONTAINERNETWORK
#define MULIB_IMPL_INET4
#define MULIB_IMPL_PARSEC
#include "mulib-ocicontainernetwork.h"
#include <assert.h>
int
main (void)
{
  mulib_ocicontainernetwork_t network;
  assert (
      mulib_ocicontainernetwork_init (&network, "10.0.0.2", 24, "10.0.0.1"));
  assert (network.has_gateway && network.prefix == 24);
  return 0;
}
