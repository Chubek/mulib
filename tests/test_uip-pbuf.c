/* Generated, standalone MuLIB add-on symbol and API test: uip-pbuf */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_UIP_PBUF
#include "mulib-uip-pbuf.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_pbuf_init;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_pbuf_destroy;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_pbuf_prepend;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_pbuf_append;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_pbuf_data;
    assert (p);
  }

  mulib_uip_pbuf_t b = { 0 };
  assert (!mulib_uip_pbuf_init (&b, 4, 40));
  assert (!mulib_uip_pbuf_append (&b, "ab", 2));
  assert (!mulib_uip_pbuf_prepend (&b, "cd", 2));
  assert (b.length == 4);
  mulib_uip_pbuf_destroy (&b);
  puts ("PASS uip-pbuf");
  return 0;
}
