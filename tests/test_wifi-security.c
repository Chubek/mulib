/* Generated, standalone MuLIB add-on symbol and API test: wifi-security */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_WIFI_SECURITY
#include "mulib-wifi-security.h"
int
main (void)
{
  {
    void (*volatile p) (void)
        = (void (*) (void))&mulib_wifi_security_rsn_parse;
    assert (p);
  }

  uint8_t rsn[] = { 48, 18, 1,   0, 0, 15, 172, 4,  1,   0,
                    0,  15, 172, 4, 1, 0,  0,   15, 172, 2 };
  mulib_wifi_security_rsn_t x;
  assert (!mulib_wifi_security_rsn_parse (rsn, sizeof rsn, &x));
  assert (x.akm_count == 1);
  puts ("PASS wifi-security");
  return 0;
}
