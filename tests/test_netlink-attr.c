/* Generated, standalone MuLIB add-on symbol and API test: netlink-attr */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK_ATTR
#include "mulib-netlink-attr.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_netlink_attr_append;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_netlink_attr_foreach;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_netlink_attr_u32;
    assert (p);
  }

  unsigned char buf[40] = { 0 };
  size_t len = 0;
  uint32_t value = 0x12345678u;
  assert (!mulib_netlink_attr_append (buf, sizeof buf, &len, 42, &value, 4));
  assert (len == 8);
  assert (mulib_netlink_attr_append (buf, 8, &len, 42, &value, 4) < 0);
  assert (mulib_netlink_attr_foreach (buf, 1, NULL, NULL) < 0);
  puts ("PASS netlink-attr");
  return 0;
}
