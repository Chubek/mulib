#define MULIB_IMPL_EVTLOOP
#include "mulib-evtloop.h"
#include <assert.h>
static void
increment (void *context)
{
  ++*(int *)context;
}
int
main (void)
{
  mulib_evtloop_t loop = { 0 };
  int count = 0;
  loop.limit = 10;
  assert (mulib_evtloop_post (&loop, 100, increment, &count));
  assert (!mulib_evtloop_run (&loop, 99, 10));
  assert (mulib_evtloop_run (&loop, 100, 10) == 1 && count == 1);
  mulib_evtloop_free (&loop);
  return 0;
}
