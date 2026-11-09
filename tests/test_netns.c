/* Generated, standalone MuLIB add-on symbol and API test: netns */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETNS
#include "mulib-netns.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_netns_open_self;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_netns_open_pid;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_netns_enter;
    assert (p);
  }

  int fd = mulib_netns_open_self ();
  assert (fd >= 0);
  close (fd);
  puts ("PASS netns");
  return 0;
}
