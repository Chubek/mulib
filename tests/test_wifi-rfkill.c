/* Generated, standalone MuLIB add-on symbol and API test: wifi-rfkill */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_WIFI_RFKILL
#include "mulib-wifi-rfkill.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_wifi_rfkill_read;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_wifi_rfkill_open;
    assert (p);
  }

  assert (mulib_wifi_rfkill_read (-1, NULL) < 0);
  puts ("PASS wifi-rfkill");
  return 0;
}
