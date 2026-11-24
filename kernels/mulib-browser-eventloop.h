/** @file mulib-browser-eventloop.h
 * @brief Deterministic bounded FIFO tasks and run-to-completion microtask
 * checkpoints. C11 single-header module. Define MULIB_IMPL_BROWSER_EVENTLOOP
 * in exactly one C translation unit.
 *
 */
#ifndef MULIB_BROWSER_EVENTLOOP_H
#define MULIB_BROWSER_EVENTLOOP_H
#include "mulib-microjs-promise.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_eventloop
  {
    mulib_microjs_microtasks_t tasks, microtasks;
    size_t max_tasks_per_turn;
  } mulib_browser_eventloop_t;
  /** @brief See manual/mulib-browser-eventloop.md for this operation's
   * contract. */
  int mulib_browser_eventloop_post (mulib_browser_eventloop_t *e,
                                    mulib_microjs_job_fn cb, void *ctx,
                                    int microtask);
  /** @brief See manual/mulib-browser-eventloop.md for this operation's
   * contract. */
  size_t mulib_browser_eventloop_step (mulib_browser_eventloop_t *e);
  /** @brief See manual/mulib-browser-eventloop.md for this operation's
   * contract. */
  void mulib_browser_eventloop_free (mulib_browser_eventloop_t *e);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_EVENTLOOP_H */
#ifdef MULIB_IMPL_BROWSER_EVENTLOOP
#ifndef MULIB_IMPL_BROWSER_EVENTLOOP_ONCE
#define MULIB_IMPL_BROWSER_EVENTLOOP_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_browser_eventloop_post (mulib_browser_eventloop_t *e,
                              mulib_microjs_job_fn f, void *ctx, int micro)
{
  if (!e)
    return 0;
  return mulib_microjs_microtask_post (micro ? &e->microtasks : &e->tasks, f,
                                       ctx);
}
size_t
mulib_browser_eventloop_step (mulib_browser_eventloop_t *e)
{
  if (!e || e->tasks.draining || e->microtasks.draining)
    return 0;
  size_t budget = e->max_tasks_per_turn ? e->max_tasks_per_turn : 10000;
  if (e->microtasks.head < e->microtasks.len)
    return mulib_microjs_microtask_drain (&e->microtasks, budget);
  size_t n = mulib_microjs_microtask_drain (&e->tasks, 1);
  return n + mulib_microjs_microtask_drain (&e->microtasks, budget);
}
void
mulib_browser_eventloop_free (mulib_browser_eventloop_t *e)
{
  if (e)
    {
      mulib_microjs_microtask_free (&e->tasks);
      mulib_microjs_microtask_free (&e->microtasks);
    }
}
#endif
#endif
