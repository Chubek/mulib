/** @file mulib-browser-stream.h
 * @brief Typed bounded event stream with channel-based observer dispatch.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_STREAM in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_BROWSER_STREAM_H
#define MULIB_BROWSER_STREAM_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum mulib_browser_channel
  {
    MULIB_BROWSER_CHANNEL_TOKEN = 1,
    MULIB_BROWSER_CHANNEL_DOM = 2,
    MULIB_BROWSER_CHANNEL_LAYOUT = 3,
    MULIB_BROWSER_CHANNEL_NETWORK = 4,
    MULIB_BROWSER_CHANNEL_EXECUTION = 5
  } mulib_browser_channel_t;
  typedef struct mulib_browser_event
  {
    mulib_browser_channel_t channel;
    unsigned code;
    uint64_t sequence, node_id;
    const void *payload;
    size_t length;
  } mulib_browser_event_t;
  typedef int (*mulib_browser_event_fn) (void *context,
                                         const mulib_browser_event_t *event);
  typedef struct mulib_browser_observer
  {
    unsigned channel_mask;
    mulib_browser_event_fn fn;
    void *ctx;
  } mulib_browser_observer_t;
  typedef struct mulib_browser_stream
  {
    mulib_browser_observer_t *observers;
    size_t len, cap, max_observers;
    uint64_t sequence;
    int dispatching;
  } mulib_browser_stream_t;
  /** @brief See manual/mulib-browser-stream.md for this operation's contract.
   */
  int mulib_browser_stream_subscribe (mulib_browser_stream_t *stream,
                                      unsigned channel_mask,
                                      mulib_browser_event_fn callback,
                                      void *ctx);
  /** @brief See manual/mulib-browser-stream.md for this operation's contract.
   */
  int mulib_browser_stream_emit (mulib_browser_stream_t *stream,
                                 mulib_browser_channel_t channel,
                                 unsigned code, uint64_t node_id,
                                 const void *data, size_t length);
  /** @brief See manual/mulib-browser-stream.md for this operation's contract.
   */
  void mulib_browser_stream_free (mulib_browser_stream_t *stream);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_STREAM_H */
#ifdef MULIB_IMPL_BROWSER_STREAM
#ifndef MULIB_IMPL_BROWSER_STREAM_ONCE
#define MULIB_IMPL_BROWSER_STREAM_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_browser_stream_subscribe (mulib_browser_stream_t *s, unsigned mask,
                                mulib_browser_event_fn fn, void *ctx)
{
  if (!s || !fn || !mask || (mask & ~31u)
      || (s->max_observers && s->len >= s->max_observers))
    return 0;
  if (s->len == s->cap)
    {
      size_t c = s->cap ? s->cap * 2 : 8;
      if (c < s->cap || c > SIZE_MAX / sizeof *s->observers)
        return 0;
      if (s->max_observers && c > s->max_observers)
        c = s->max_observers;
      void *p = realloc (s->observers, c * sizeof *s->observers);
      if (!p)
        return 0;
      s->observers = p;
      s->cap = c;
    }
  s->observers[s->len++] = (mulib_browser_observer_t){ mask, fn, ctx };
  return 1;
}
int
mulib_browser_stream_emit (mulib_browser_stream_t *s,
                           mulib_browser_channel_t channel, unsigned code,
                           uint64_t node, const void *data, size_t n)
{
  if (!s || s->dispatching || channel < 1 || channel > 5 || (!data && n)
      || s->sequence == UINT64_MAX)
    return 0;
  mulib_browser_event_t e = { channel, code, ++s->sequence, node, data, n };
  size_t count = s->len;
  int ok = 1;
  s->dispatching = 1;
  for (size_t i = 0; i < count; i++)
    {
      mulib_browser_observer_t observer = s->observers[i];
      if (observer.channel_mask & (1u << (unsigned)(channel - 1)))
        if (!observer.fn (observer.ctx, &e))
          {
            ok = 0;
            break;
          }
    }
  s->dispatching = 0;
  return ok;
}
void
mulib_browser_stream_free (mulib_browser_stream_t *s)
{
  if (s && !s->dispatching)
    {
      free (s->observers);
      memset (s, 0, sizeof *s);
    }
}
#endif
#endif
