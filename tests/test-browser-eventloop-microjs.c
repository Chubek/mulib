#include <stdint.h>
#include <stdlib.h>

static size_t allocation_budget = SIZE_MAX;

static int
allow_allocation (void)
{
  if (allocation_budget == SIZE_MAX)
    return 1;
  if (!allocation_budget)
    return 0;
  --allocation_budget;
  return 1;
}

static void *
checked_malloc (size_t n)
{
  return allow_allocation () ? malloc (n) : NULL;
}

static void *
checked_realloc (void *p, size_t n)
{
  return allow_allocation () ? realloc (p, n) : NULL;
}

#define malloc checked_malloc
#define realloc checked_realloc
#define MULIB_IMPL_MINIVM
#define MULIB_IMPL_MICROJS
#define MULIB_IMPL_MICROJS_PROMISE
#define MULIB_BROWSER_EVENTLOOP_MICROJS
#define MULIB_IMPL_BROWSER_EVENTLOOP
#include "mulib-browser-eventloop.h"
#undef realloc
#undef malloc
#include "mulib-browser-checkpoint.h"
#include "mulib-browser-renderer.h"
#include <assert.h>
#include <string.h>

typedef struct
{
  mulib_browser_eventloop_t loop;
  mulib_microjs_t *vm;
  char order[16];
  size_t count;
  unsigned errors;
  mulib_microjs_status_t last_error;
  unsigned published;
} fixture_t;

static void
record (fixture_t *s, char c)
{
  assert (s->count + 1 < sizeof s->order);
  s->order[s->count++] = c;
}

static void
native_d (void *ctx)
{
  record (ctx, 'D');
}

static void
native_a (void *ctx)
{
  fixture_t *s = ctx;
  record (s, 'A');
  assert (mulib_browser_eventloop_post (&s->loop, native_d, s, 1));
}

static void
native_b (void *ctx)
{
  record (ctx, 'B');
}

static void
native_n (void *ctx)
{
  record (ctx, 'N');
}

static void
native_r (void *ctx)
{
  record (ctx, 'R');
}

static void
report (void *ctx, const mulib_microjs_error_t *error)
{
  fixture_t *s = ctx;
  assert (!mulib_browser_eventloop_at_checkpoint (&s->loop));
  assert (mulib_browser_eventloop_step (&s->loop) == 0);
  ++s->errors;
  s->last_error = error->code;
  if (error->code == MULIB_MICROJS_EXCEPTION)
    {
      record (s, 'E');
      assert (mulib_browser_eventloop_post (&s->loop, native_r, s, 1));
    }
}

static mulib_microjs_status_t
enqueue (mulib_microjs_t *vm, const mulib_microjs_value_t *args, size_t count,
         mulib_microjs_value_t *out, void *ctx)
{
  fixture_t *s = ctx;
  if (count != 1 || args[0].type != MULIB_MICROJS_FUNCTION)
    return MULIB_MICROJS_TYPE;
  assert (mulib_browser_eventloop_post_microjs (&s->loop, vm, args, report, s));
  assert (mulib_browser_eventloop_post (&s->loop, native_n, s, 1));
  out->type = MULIB_MICROJS_UNDEFINED;
  return MULIB_MICROJS_OK;
}

static mulib_microjs_status_t
mark (mulib_microjs_t *vm, const mulib_microjs_value_t *args, size_t count,
      mulib_microjs_value_t *out, void *ctx)
{
  fixture_t *s = ctx;
  assert (vm == s->vm && count == 1 && args[0].type == MULIB_MICROJS_NUMBER);
  assert (args[0].number >= 'A' && args[0].number <= 'Z');
  assert (!mulib_browser_eventloop_at_checkpoint (&s->loop));
  /* A callback cannot discard any queued native or JavaScript work. */
  mulib_browser_eventloop_free (&s->loop);
  assert (mulib_browser_eventloop_step (&s->loop) == 0);
  record (s, (char)args[0].number);
  out->type = MULIB_MICROJS_UNDEFINED;
  return MULIB_MICROJS_OK;
}

static mulib_microjs_value_t
closure (mulib_microjs_t *vm, const char *source)
{
  mulib_microjs_value_t fn = { 0 };
  assert (mulib_microjs_eval (vm, source, strlen (source), &fn, NULL)
          == MULIB_MICROJS_OK);
  assert (fn.type == MULIB_MICROJS_FUNCTION);
  return fn;
}

static void
first_task (void *ctx)
{
  fixture_t *s = ctx;
  assert (!mulib_browser_eventloop_at_checkpoint (&s->loop));
  record (s, 'T');
  /* The closure and its captured object have no global binding; they survive
   * solely via the queued root after releasing the returned handle. */
  mulib_microjs_value_t fn = closure (
      s->vm, "(function() { let d={x:74}; return function(){"
             "mark(d.x); enqueue(function(){mark(d.x+14);}); }; })()");
  assert (mulib_browser_eventloop_post (&s->loop, native_a, s, 1));
  assert (mulib_browser_eventloop_post_microjs (&s->loop, s->vm, &fn, report, s));
  mulib_microjs_value_free (&fn);
  assert (mulib_browser_eventloop_post (&s->loop, native_b, s, 1));
  fn = closure (s->vm, "(function(){ throw 'fixture'; })");
  assert (mulib_browser_eventloop_post_microjs (&s->loop, s->vm, &fn, report, s));
  mulib_microjs_value_free (&fn);
  (void)mulib_microjs_collect (s->vm);
}

static int
render (void *ctx, const mulib_browser_wire_event *e)
{
  fixture_t *s = ctx;
  assert (e->seq == 1 && e->document == 7 && e->payload.len == 9);
  assert (memcmp (e->payload.data, "TAJBEDXNR", 9) == 0);
  ++s->published;
  return 1;
}

static int
publish (fixture_t *s, mulib_browser_renderer_t *r)
{
  if (!mulib_browser_eventloop_at_checkpoint (&s->loop))
    return 0;
  mulib_browser_journal_event entries[1] = { 0 };
  mulib_browser_journal_t journal = { entries, 0, 1, 0, sizeof s->order, 0 };
  assert (s->count > 0);
  assert (mulib_browser_journal_append (&journal, 8, 1, 7, 0,
                                        (muba_span){ (unsigned char *)s->order,
                                                     s->count }));
  mulib_browser_checkpoint_t point;
  assert (mulib_browser_checkpoint_capture (&journal, 7, 1, &point));
  const mulib_browser_journal_event *entry = &entries[0];
  mulib_browser_wire_event e = {
    point.sequence, point.document, entry->node, entry->channel, entry->kind,
    { entry->payload.data, entry->payload.len }
  };
  int ok = mulib_browser_renderer_publish (r, &e);
  mulib_browser_journal_clear (&journal);
  return ok;
}

static void
next_task (void *ctx)
{
  fixture_t *s = ctx;
  assert (s->published == 1 && strcmp (s->order, "TAJBEDXNR") == 0);
  record (s, 'Z');
}

static void
mixed_checkpoint (void)
{
  fixture_t s = { 0 };
  s.vm = mulib_microjs_create (NULL);
  assert (s.vm);
  s.loop.microtasks.limit = 8;
  s.loop.max_tasks_per_turn = 2;
  assert (mulib_microjs_bind (s.vm, "mark", mark, &s) == MULIB_MICROJS_OK);
  assert (mulib_microjs_bind (s.vm, "enqueue", enqueue, &s) == MULIB_MICROJS_OK);
  assert (mulib_browser_eventloop_post (&s.loop, first_task, &s, 0));
  assert (mulib_browser_eventloop_post (&s.loop, next_task, &s, 0));
  mulib_browser_renderer_t renderer = { 0 };
  assert (mulib_browser_renderer_attach (&renderer, 1, 1, 1, render, &s));

  assert (mulib_browser_eventloop_step (&s.loop) == 3);
  assert (strcmp (s.order, "TAJ") == 0 && !publish (&s, &renderer));
  assert (mulib_browser_eventloop_step (&s.loop) == 2);
  assert (strcmp (s.order, "TAJBE") == 0 && !publish (&s, &renderer));
  assert (s.errors == 1 && s.last_error == MULIB_MICROJS_EXCEPTION);
  assert (mulib_browser_eventloop_step (&s.loop) == 2);
  assert (strcmp (s.order, "TAJBEDX") == 0 && !publish (&s, &renderer));
  assert (mulib_browser_eventloop_step (&s.loop) == 2);
  assert (publish (&s, &renderer));
  assert (mulib_browser_eventloop_step (&s.loop) == 1);
  assert (strcmp (s.order, "TAJBEDXNRZ") == 0);
  mulib_browser_eventloop_free (&s.loop);
  mulib_microjs_destroy (s.vm);
}

static void
teardown (void *ctx)
{
  fixture_t *s = ctx;
  record (s, 'C');
  assert (!mulib_browser_eventloop_post (&s->loop, native_a, s, 1));
  assert (!mulib_browser_eventloop_post (&s->loop, native_a, s, 0));
  assert (!mulib_browser_eventloop_post_owned (&s->loop, native_b, s, teardown));
  assert (!mulib_browser_eventloop_at_checkpoint (&s->loop));
  assert (!mulib_browser_eventloop_step (&s->loop));
  mulib_browser_eventloop_free (&s->loop);
}

static void
rejection_and_release (void)
{
  fixture_t s = { 0 };
  s.vm = mulib_microjs_create (NULL);
  mulib_microjs_t *other = mulib_microjs_create (NULL);
  assert (s.vm && other);
  mulib_microjs_value_t fn = closure (s.vm, "(function(){throw 1;})");
  const mulib_microjs_value_t number = { .type = MULIB_MICROJS_NUMBER };
  assert (!mulib_browser_eventloop_post_microjs (&s.loop, s.vm, &fn, report, &s));
  s.loop.microtasks.limit = 1;
  assert (!mulib_browser_eventloop_post_microjs (&s.loop, s.vm, &number, report, &s));
  assert (!mulib_browser_eventloop_post_microjs (&s.loop, other, &fn, report, &s));
  /* Fail at each of the three allocations before the first successful post:
   * the adapter context, the owned trampoline and the native FIFO growth. */
  for (size_t i = 0; i != 3; ++i)
    {
      allocation_budget = i;
      assert (!mulib_browser_eventloop_post_microjs (&s.loop, s.vm, &fn, report, &s));
      assert (mulib_browser_eventloop_at_checkpoint (&s.loop));
    }
  allocation_budget = SIZE_MAX;
  assert (mulib_browser_eventloop_at_checkpoint (&s.loop));
  assert (mulib_browser_eventloop_post_microjs (&s.loop, s.vm, &fn, report, &s));
  assert (!mulib_browser_eventloop_post_microjs (&s.loop, s.vm, &fn, report, &s));
  assert (!mulib_browser_eventloop_post_owned (&s.loop, native_b, &s, teardown));
  s.loop.microtasks.limit = 4;
  assert (mulib_browser_eventloop_post_owned (&s.loop, native_b, &s, teardown));
  assert (mulib_browser_eventloop_post_microjs (&s.loop, s.vm, &fn, report, &s));
  mulib_microjs_value_free (&fn);
  mulib_microjs_destroy (s.vm);
  s.loop.max_tasks_per_turn = 1;
  assert (mulib_browser_eventloop_step (&s.loop) == 1);
  assert (s.errors == 1 && s.last_error == MULIB_MICROJS_INVALID);
  /* The second rooted job ensures the destroyed interpreter's storage still
   * exists. Free invokes the owned cleanup but neither pending callback. */
  mulib_browser_eventloop_free (&s.loop);
  assert (strcmp (s.order, "C") == 0 && s.errors == 1);
  assert (mulib_browser_eventloop_at_checkpoint (&s.loop));
  mulib_browser_eventloop_free (&s.loop);
  mulib_microjs_destroy (other);
}

int
main (void)
{
  mixed_checkpoint ();
  rejection_and_release ();
  return 0;
}
