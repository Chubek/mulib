/* Complete, offline browser-networkstream example. */
#define MULIB_IMPL_BROWSER_NETWORKSTREAM
#define MULIB_IMPL_BROWSER_STREAM
#include "mulib-browser-networkstream.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static int
on_event (void *ctx, const mulib_browser_event_t *event)
{
  assert (event->sequence > 0);
  (*(size_t *)ctx)++;
  return 1;
}

int
main (void)
{
  mulib_browser_stream_t stream = { 0 };
  size_t count = 0;
  assert (mulib_browser_stream_subscribe (&stream, 8, on_event, &count));
  assert (mulib_browser_networkstream_emit (&stream, 1, 1, "event", 5)
          && count == 1);
  mulib_browser_stream_free (&stream);
  return 0;
}
