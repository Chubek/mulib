#define MULIB_IMPL_NET_PROFILE
#include "mulib-net-profile.h"
#include <assert.h>
int
main (void)
{
  const char text[] = "interface=lo\nenabled=1\n";
  mulib_net_profile_t profile;
  assert (!mulib_net_profile_parse (text, sizeof text - 1, &profile));
  assert (!strcmp (profile.interface, "lo"));
  return 0;
}
