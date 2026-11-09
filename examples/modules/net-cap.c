#define MULIB_IMPL_NET_CAP
#include "mulib-net-cap.h"
#include <assert.h>
#include <inttypes.h>
int
main (void)
{
  uint64_t bits = 0;
  assert (!mulib_net_cap_effective (&bits));
  printf ("effective=%" PRIx64 "\n", bits);
  return 0;
}
