/* Generated, standalone MuLIB add-on symbol and API test: net-profile */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NET_PROFILE
#include "mulib-net-profile.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_net_profile_parse;
    assert (p);
  }

  const char t[] = "interface=lo\nenabled=1\n";
  mulib_net_profile_t p;
  assert (!mulib_net_profile_parse (t, strlen (t), &p));
  assert (!strcmp (p.interface, "lo"));
  puts ("PASS net-profile");
  return 0;
}
