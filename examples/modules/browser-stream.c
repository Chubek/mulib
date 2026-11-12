/* Complete, offline browser-stream example. */
#define MULIB_IMPL_BROWSER_STREAM
#include "mulib-browser-stream.h"
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
  assert (mulib_browser_stream_subscribe (&stream, 1, on_event, &count));
  assert (mulib_browser_stream_emit (&stream, MULIB_BROWSER_CHANNEL_TOKEN, 1,
                                     0, "x", 1)
          && count == 1);
  mulib_browser_stream_free (&stream);
  return 0;
}
