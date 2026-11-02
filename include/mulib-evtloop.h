/** @file mulib-evtloop.h
 * @brief Cooperative caller-clocked one-shot task/timer queue. No polling I/O.
 * Define MULIB_IMPL_EVTLOOP. Zero initialize; set nonzero limit before use.
 * Not thread-safe. Callbacks may post/cancel, but must not free the loop. */
#ifndef MULIB_EVTLOOP_H
#define MULIB_EVTLOOP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef void (*mulib_evtloop_fn) (void *context);
  typedef struct mulib_evtloop_task
  {
    uint64_t id, due;
    mulib_evtloop_fn run;
    void *context;
  } mulib_evtloop_task_t;
  typedef struct mulib_evtloop
  {
    mulib_evtloop_task_t *tasks;
    size_t count, limit;
    uint64_t serial, now;
    int running;
  } mulib_evtloop_t;
  /** Absolute due time. Return nonzero cancellation id, zero invalid/full/OOM.
   */
  uint64_t mulib_evtloop_post (mulib_evtloop_t *loop, uint64_t due,
                               mulib_evtloop_fn run, void *context);
  int mulib_evtloop_cancel (mulib_evtloop_t *loop, uint64_t id);
  /** Execute at most budget tasks due <= now, in due/id order. Backward time or
   * recursive run returns SIZE_MAX. Tasks are removed before callback. */
  size_t mulib_evtloop_run (mulib_evtloop_t *loop, uint64_t now, size_t budget);
  void mulib_evtloop_free (mulib_evtloop_t *loop);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EVTLOOP) && !defined(MULIB_EVTLOOP_IMPLEMENTED)
#define MULIB_EVTLOOP_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
uint64_t
mulib_evtloop_post (mulib_evtloop_t *l, uint64_t due, mulib_evtloop_fn f,
                    void *ctx)
{
  mulib_evtloop_task_t *p;
  if (!l || !f || l->count >= l->limit || l->count >= SIZE_MAX / sizeof (*p)
      || l->serial == UINT64_MAX)
    return 0;
  p = (mulib_evtloop_task_t *)realloc (l->tasks, (l->count + 1) * sizeof (*p));
  if (!p)
    return 0;
  l->tasks = p;
  p += l->count++;
  p->id = ++l->serial;
  p->due = due;
  p->run = f;
  p->context = ctx;
  return p->id;
}
int
mulib_evtloop_cancel (mulib_evtloop_t *l, uint64_t id)
{
  if (!l || !id)
    return 0;
  for (size_t i = 0; i < l->count; ++i)
    if (l->tasks[i].id == id)
      {
        l->tasks[i] = l->tasks[--l->count];
        return 1;
      }
  return 0;
}
size_t
mulib_evtloop_run (mulib_evtloop_t *l, uint64_t now, size_t budget)
{
  size_t done = 0;
  if (!l || l->running || now < l->now)
    return SIZE_MAX;
  l->now = now;
  l->running = 1;
  while (done < budget)
    {
      size_t best = SIZE_MAX;
      mulib_evtloop_task_t task;
      for (size_t i = 0; i < l->count; ++i)
        if (l->tasks[i].due <= now
            && (best == SIZE_MAX || l->tasks[i].due < l->tasks[best].due
                || (l->tasks[i].due == l->tasks[best].due
                    && l->tasks[i].id < l->tasks[best].id)))
          best = i;
      if (best == SIZE_MAX)
        break;
      task = l->tasks[best];
      l->tasks[best] = l->tasks[--l->count];
      ++done;
      task.run (task.context);
    }
  l->running = 0;
  return done;
}
void
mulib_evtloop_free (mulib_evtloop_t *l)
{
  if (l && !l->running)
    {
      free (l->tasks);
      memset (l, 0, sizeof (*l));
    }
}
#endif
