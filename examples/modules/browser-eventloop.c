/* Complete, offline browser-eventloop example. */
#define MULIB_IMPL_BROWSER_EVENTLOOP
#define MULIB_IMPL_MICROJS_PROMISE
#include "mulib-browser-eventloop.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static void
increment (void *ctx)
{
  (*(unsigned *)ctx)++;
}

int
main (void)
{
  mulib_browser_eventloop_t loop = { 0 };
  unsigned count = 0;
  assert (mulib_browser_eventloop_post (&loop, increment, &count, 0));
  assert (mulib_browser_eventloop_step (&loop) == 1 && count == 1);
  mulib_browser_eventloop_free (&loop);
  return 0;
}
