/* Generated, standalone MuLIB add-on symbol and API test: netlink-schema */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NETLINK_SCHEMA
#include "mulib-netlink-schema.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_netlink_schema_check;
    assert (p);
  }

  mulib_netlink_policy_t p = { 4, 2, 8, 1 };
  assert (mulib_netlink_schema_check (NULL, 0, &p, 1) == -ENOENT);
  puts ("PASS netlink-schema");
  return 0;
}
