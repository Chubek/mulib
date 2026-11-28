#define MULIB_IMPL_BROWSER_EVENTLOOP
#define MULIB_IMPL_MICROJS_PROMISE
#include "mulib-browser-eventloop.h"
#include <assert.h>
#include <string.h>

typedef struct
{
  mulib_browser_eventloop_t loop;
  char order[6];
  size_t count;
} test_state_t;

static void
record (test_state_t *s, char marker)
{
  assert (s->count < sizeof s->order - 1);
  s->order[s->count++] = marker;
}

static void
last_task (void *ctx)
{
  record (ctx, 'E');
}

static void
last_microtask (void *ctx)
{
  record (ctx, 'D');
}

static void
first_microtask (void *ctx)
{
  test_state_t *s = ctx;
  record (s, 'B');
  mulib_browser_eventloop_free (&s->loop);
  assert (mulib_browser_eventloop_step (&s->loop) == 0);
}

static void
second_microtask (void *ctx)
{
  record (ctx, 'C');
}

static void
first_task (void *ctx)
{
  test_state_t *s = ctx;
  record (s, 'A');
  assert (mulib_browser_eventloop_post (&s->loop, first_microtask, s, 1));
  assert (mulib_browser_eventloop_post (&s->loop, second_microtask, s, 1));
  mulib_browser_eventloop_free (&s->loop);
  assert (mulib_browser_eventloop_step (&s->loop) == 0);
  assert (mulib_browser_eventloop_post (&s->loop, last_microtask, s, 1));
}

int
main (void)
{
  test_state_t s = { 0 };
  s.loop.max_tasks_per_turn = 2;
  assert (mulib_browser_eventloop_post (&s.loop, first_task, &s, 0));
  assert (mulib_browser_eventloop_post (&s.loop, last_task, &s, 0));
  assert (mulib_browser_eventloop_step (&s.loop) == 3);
  assert (strcmp (s.order, "ABC") == 0);
  assert (mulib_browser_eventloop_step (&s.loop) == 1);
  assert (strcmp (s.order, "ABCD") == 0);
  assert (mulib_browser_eventloop_step (&s.loop) == 1);
  assert (strcmp (s.order, "ABCDE") == 0);
  assert (mulib_browser_eventloop_step (&s.loop) == 0);
  mulib_browser_eventloop_free (&s.loop);
  assert (!s.loop.tasks.jobs && !s.loop.microtasks.jobs);
  return 0;
}
