#include "fixtures/web_primitives_impl.h"
#define MULIB_IMPL_WEBTIMERS
#include "mulib-webtimers.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

static const char a[] = "https://example.com/a";
static const char b[] = "https://example.com/b";
static const char c[] = "https://example.com/c";

typedef struct
{
  mulib_browser_engine_t engine;
  mulib_browser_eventloop_t loop;
  unsigned status, calls;
  int fail, trace;
  char order[4];
  size_t count;
} state_t;

static void
record (state_t *s, char mark)
{
  assert (s->count < sizeof s->order - 1);
  s->order[s->count++] = mark;
}

static void
check_page (state_t *s, size_t current, const char *tag)
{
  assert (s->engine.history.current == current);
  assert (s->engine.document && s->engine.document->first_child);
  assert (strcmp (s->engine.document->first_child->tag, tag) == 0);
}

static int
transport (void *ctx, const char *url, mulib_bounded_text_buffer_t *body,
           unsigned *status, char *type, size_t cap)
{
  state_t *s = ctx;
  s->calls++;
  if (s->trace)
    {
      assert (strcmp (url, a) == 0);
      check_page (s, 1, "aside");
      assert (s->status == 999);
      assert (!mulib_browser_engine_traverse (&s->engine, 0, &s->status));
      mulib_browser_engine_destroy (&s->engine);
      check_page (s, 1, "aside");
    }
  *status = s->fail ? 404 : 200;
  const char *markup = strcmp (url, a) == 0 ? "<main></main>"
                       : strcmp (url, b) == 0 ? "<aside></aside>"
                                              : "<footer></footer>";
  return mulib_bounded_text_copy (type, cap, "text/html")
         && mulib_bounded_text_text (body, markup);
}

static void
checkpoint (void *ctx)
{
  state_t *s = ctx;
  check_page (s, 0, "main");
  record (s, 'M');
}

static int
observe (void *ctx, const mulib_browser_event_t *event)
{
  state_t *s = ctx;
  if (event->channel != MULIB_BROWSER_CHANNEL_NETWORK)
    {
      unsigned calls = s->calls;
      assert (!mulib_browser_engine_navigate (&s->engine, a, &s->status));
      assert (!mulib_browser_engine_traverse (&s->engine, 0, &s->status));
      assert (s->calls == calls);
      return 1;
    }
  if (s->trace)
    {
      check_page (s, 0, "main");
      assert (s->status == 200 && event->code == 200);
      assert (event->length == strlen (a)
              && memcmp (event->payload, a, event->length) == 0);
      assert (event->sequence == 3 && s->engine.history.len == 2);
      assert (!mulib_browser_engine_navigate (&s->engine, c, &s->status));
      mulib_browser_engine_destroy (&s->engine);
      check_page (s, 0, "main");
      record (s, 'O');
      assert (mulib_browser_eventloop_post (&s->loop, checkpoint, s, 1));
    }
  return 0; /* Rejection is advisory, including on traversal. */
}

static void
back_task (void *ctx)
{
  state_t *s = ctx;
  assert (mulib_browser_engine_traverse (&s->engine, -1, &s->status));
  record (s, 'T');
}

static void
after_task (void *ctx)
{
  assert (strcmp (((state_t *)ctx)->order, "OTM") == 0);
}

int
main (void)
{
  state_t s = { 0 };
  mulib_browser_fetch_t fetch = { 0 };
  fetch.client.transport = transport;
  fetch.client.ctx = &s;
  fetch.client.policy.allow_https = 1;
  assert (mulib_browser_origin_parse (a, &fetch.initiator));
  fetch.enforce_same_origin = 1;
  mulib_browser_limits_t limits;
  assert (mulib_browser_limits_default (&limits));
  limits.max_urls = 2;
  assert (mulib_browser_engine_init (&s.engine, &fetch, &limits));
  assert (!mulib_browser_engine_traverse (&s.engine, 0, &s.status));
  assert (mulib_browser_stream_subscribe (&s.engine.stream, 31, observe, &s));
  assert (mulib_browser_engine_navigate (&s.engine, a, &s.status));
  assert (mulib_browser_engine_navigate (&s.engine, b, &s.status));
  check_page (&s, 1, "aside");

  s.trace = s.fail = 1;
  s.status = 999;
  assert (!mulib_browser_engine_traverse (&s.engine, INT_MIN, &s.status));
  assert (!mulib_browser_engine_traverse (&s.engine, 1, &s.status));
  assert (s.calls == 2);
  mulib_html_tree_node_t *old = s.engine.document;
  assert (!mulib_browser_engine_traverse (&s.engine, -1, &s.status));
  assert (s.engine.document == old && s.engine.history.current == 1);
  assert (s.status == 999 && s.engine.stream.sequence == 2 && s.calls == 3);
  s.fail = 0;

  mulib_webtimers_t timers = { 0 };
  uint64_t id = mulib_webtimers_add (&timers, 100, 5);
  assert (id && !mulib_webtimers_next (&timers, 104));
  assert (!mulib_browser_eventloop_step (&s.loop) && s.calls == 3);
  assert (mulib_webtimers_next (&timers, 105) == id);
  assert (mulib_browser_eventloop_post (&s.loop, back_task, &s, 0));
  assert (mulib_browser_eventloop_post (&s.loop, after_task, &s, 0));
  assert (mulib_browser_eventloop_step (&s.loop) == 2);
  assert (mulib_browser_eventloop_step (&s.loop) == 1);
  assert (mulib_browser_eventloop_step (&s.loop) == 0 && s.calls == 4);
  s.trace = 0;

  assert (mulib_browser_engine_traverse (&s.engine, 1, &s.status));
  check_page (&s, 1, "aside");
  assert (mulib_browser_engine_traverse (&s.engine, 0, &s.status));
  assert (s.engine.history.len == 2);
  assert (mulib_browser_engine_traverse (&s.engine, -1, &s.status));
  assert (mulib_browser_engine_navigate (&s.engine, c, &s.status));
  check_page (&s, 1, "footer");
  assert (s.engine.history.len == 2 && strcmp (s.engine.history.urls[1], c) == 0);
  assert (!mulib_browser_engine_traverse (&s.engine, 1, &s.status));
  assert (mulib_browser_stream_emit (&s.engine.stream, MULIB_BROWSER_CHANNEL_DOM,
                                     0, 0, NULL, 0));
  check_page (&s, 1, "footer");
  mulib_browser_eventloop_free (&s.loop);
  mulib_browser_engine_destroy (&s.engine);
  return 0;
}
