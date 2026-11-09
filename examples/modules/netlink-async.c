#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ASYNC
#include "mulib-netlink-async.h"
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
int
main (void)
{
  int pair[2];
  assert (!socketpair (AF_UNIX, SOCK_STREAM, 0, pair));
  assert (mulib_netlink_async_wait (pair[0], 0, 0) == -ETIMEDOUT);
  close (pair[0]);
  close (pair[1]);
  return 0;
}
