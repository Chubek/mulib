#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_RTNL
#define MULIB_IMPL_RTNL_LINK
#include "mulib-rtnl-link.h"
#include <assert.h>
int
main (void)
{
  assert (mulib_rtnl_link_index ("lo") > 0);
  return 0;
}
