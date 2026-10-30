#define MULIB_IMPL_NETDEVICE
#include "mulib-netdevice.h"
#include <assert.h>
#if defined(__unix__) || defined(__APPLE__)
static int
visit (void *ctx, const mulib_netdevice_info_t *v)
{
  ++*(unsigned *)ctx;
  assert (v->name && v->index);
  return 0;
}
#endif
int
main (void)
{
#if defined(__unix__) || defined(__APPLE__)
  unsigned count = 0;
  assert (!mulib_netdevice_list (visit, &count));
  assert (count);
#endif
  return 0;
}
