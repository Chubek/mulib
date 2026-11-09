/* Generated, standalone MuLIB add-on symbol and API test: wpa-ctrl */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_WPA_CTRL
#include "mulib-wpa-ctrl.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_wpa_ctrl_request;
    assert (p);
  }

  char b[16];
  size_t n = 0;
  assert (mulib_wpa_ctrl_request ("../../evil", "PING", b, sizeof b, &n)
          == -EINVAL);
  puts ("PASS wpa-ctrl");
  return 0;
}
