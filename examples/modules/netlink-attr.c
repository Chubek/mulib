#define MULIB_IMPL_NETLINK_ATTR
#include "mulib-netlink-attr.h"
#include <assert.h>
int
main (void)
{
  unsigned char bytes[16];
  size_t used = 0;
  uint32_t value = 42;
  assert (
      !mulib_netlink_attr_append (bytes, sizeof bytes, &used, 1, &value, 4));
  assert (used == 8);
  return 0;
}
