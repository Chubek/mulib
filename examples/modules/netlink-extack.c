#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_EXTACK
#include "mulib-netlink-extack.h"
#include <assert.h>
int
main (void)
{
  struct
  {
    struct nlmsghdr h;
    struct nlmsgerr e;
  } frame = { 0 };
  frame.h.nlmsg_len = sizeof frame;
  frame.h.nlmsg_type = NLMSG_ERROR;
  frame.e.error = -EINVAL;
  mulib_netlink_error_t error;
  assert (!mulib_netlink_extack_parse (&frame, sizeof frame, &error));
  assert (error.code == -EINVAL);
  return 0;
}
