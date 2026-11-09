#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NETLINK_SCHEMA
#include "mulib-netlink-schema.h"
#include <assert.h>
int
main (void)
{
  unsigned char bytes[16];
  size_t used = 0;
  uint32_t value = 42;
  mulib_netlink_policy_t rule = { 1, 4, 4, 1 };
  assert (
      !mulib_netlink_attr_append (bytes, sizeof bytes, &used, 1, &value, 4));
  assert (!mulib_netlink_schema_check (bytes, used, &rule, 1));
  return 0;
}
