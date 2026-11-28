/** @file mulib-browser-eventloop.h
 * @brief Deterministic bounded FIFO tasks and run-to-completion microtask
 * checkpoints. C11 single-header module. Define MULIB_IMPL_BROWSER_EVENTLOOP
 * in exactly one C translation unit.
 *
 */
#ifndef MULIB_BROWSER_EVENTLOOP_H
#define MULIB_BROWSER_EVENTLOOP_H
#include "mulib-microjs-promise.h"
#ifdef MULIB_BROWSER_EVENTLOOP_MICROJS
#include "mulib-microjs.h"
#endif
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

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
  /** @brief Post an owned microtask. On success, cleanup runs once after cb
   * or on loop teardown if cb has not run. On failure, ownership stays with
   * the caller. Cleanup must not manipulate the queues directly. */
  int mulib_browser_eventloop_post_owned (mulib_browser_eventloop_t *e,
                                          mulib_microjs_job_fn cb, void *ctx,
                                          mulib_microjs_job_fn cleanup);
  /** @brief See manual/mulib-browser-eventloop.md for this operation's
   * contract. */
  size_t mulib_browser_eventloop_step (mulib_browser_eventloop_t *e);
  /** @brief True outside callbacks if the loop's microtask FIFO is empty.
   * Pending tasks are allowed. This is a local publication gate, not a
   * snapshot or a check of any standalone MicroJS queues. */
  int mulib_browser_eventloop_at_checkpoint (const mulib_browser_eventloop_t *e);
  /** @brief See manual/mulib-browser-eventloop.md for this operation's
   * contract. */
  void mulib_browser_eventloop_free (mulib_browser_eventloop_t *e);

#ifdef MULIB_BROWSER_EVENTLOOP_MICROJS
  typedef void (*mulib_browser_eventloop_job_error_fn) (
      void *, const mulib_microjs_error_t *);

  typedef struct mulib_browser_eventloop_js_job_
  {
    mulib_microjs_t *vm;
    mulib_microjs_value_t function;
    mulib_browser_eventloop_job_error_fn report;
    void *report_ctx;
  } mulib_browser_eventloop_js_job_;

  static inline void
  mulib_browser_eventloop_js_run_ (void *ctx)
  {
    mulib_browser_eventloop_js_job_ *job
        = (mulib_browser_eventloop_js_job_ *)ctx;
    mulib_microjs_error_t error;
    mulib_microjs_value_t result = { MULIB_MICROJS_NULL, 0, 0, NULL, 0, NULL };
    if (mulib_microjs_call (job->vm, &job->function, NULL, 0, &result, &error)
            != MULIB_MICROJS_OK
        && job->report)
      job->report (job->report_ctx, &error);
    mulib_microjs_value_free (&result);
  }

  static inline void
  mulib_browser_eventloop_js_cleanup_ (void *ctx)
  {
    mulib_browser_eventloop_js_job_ *job
        = (mulib_browser_eventloop_js_job_ *)ctx;
    mulib_microjs_value_free (&job->function);
    free (job);
  }

  /** @brief Root a trusted function and append it to the native microtask
   * FIFO. Requires a nonzero microtasks.limit. Each accepted job consumes one
   * checkpoint unit. A failed call is reported (if requested) and consumed;
   * other entries continue in FIFO order. report_ctx is borrowed. */
  static inline int
  mulib_browser_eventloop_post_microjs (mulib_browser_eventloop_t *e,
                                        mulib_microjs_t *vm,
                                        const mulib_microjs_value_t *function,
                                        mulib_browser_eventloop_job_error_fn report,
                                        void *report_ctx)
  {
    if (!e || !vm || !function || function->type != MULIB_MICROJS_FUNCTION
        || !e->microtasks.limit
        || e->microtasks.len - e->microtasks.head >= e->microtasks.limit)
      return 0;
    mulib_browser_eventloop_js_job_ *job
        = (mulib_browser_eventloop_js_job_ *)malloc (sizeof *job);
    if (!job)
      return 0;
    const mulib_microjs_value_t empty = { MULIB_MICROJS_NULL, 0, 0, NULL, 0,
                                          NULL };
    job->vm = vm;
    job->function = empty;
    job->report = report;
    job->report_ctx = report_ctx;
    if (mulib_microjs_value_copy (vm, function, &job->function)
            != MULIB_MICROJS_OK
        || !mulib_browser_eventloop_post_owned (
            e, mulib_browser_eventloop_js_run_, job,
            mulib_browser_eventloop_js_cleanup_))
      {
        mulib_browser_eventloop_js_cleanup_ (job);
        return 0;
      }
    return 1;
  }
#endif /* MULIB_BROWSER_EVENTLOOP_MICROJS */
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

/* Keep ownership metadata out of the existing public queue layout. Only
 * jobs posted by post_owned use this private trampoline. */
typedef struct
{
  mulib_microjs_job_fn run, cleanup;
  void *ctx;
} mulib_browser_eventloop_owned_;

enum { MULIB_BROWSER_EVENTLOOP_FREEING_ = 2 };

static void
mulib_browser_eventloop_owned_run_ (void *ctx)
{
  mulib_browser_eventloop_owned_ *job = (mulib_browser_eventloop_owned_ *)ctx;
  job->run (job->ctx);
  job->cleanup (job->ctx);
  free (job);
}

int
mulib_browser_eventloop_post (mulib_browser_eventloop_t *e,
                              mulib_microjs_job_fn f, void *ctx, int micro)
{
  if (!e || e->microtasks.draining == MULIB_BROWSER_EVENTLOOP_FREEING_)
    return 0;
  return mulib_microjs_microtask_post (micro ? &e->microtasks : &e->tasks, f,
                                       ctx);
}
int
mulib_browser_eventloop_post_owned (mulib_browser_eventloop_t *e,
                                    mulib_microjs_job_fn f, void *ctx,
                                    mulib_microjs_job_fn cleanup)
{
  if (!e || !f || !cleanup
      || e->microtasks.draining == MULIB_BROWSER_EVENTLOOP_FREEING_)
    return 0;
  mulib_browser_eventloop_owned_ *job
      = (mulib_browser_eventloop_owned_ *)malloc (sizeof *job);
  if (!job)
    return 0;
  *job = (mulib_browser_eventloop_owned_){ f, cleanup, ctx };
  if (!mulib_microjs_microtask_post (&e->microtasks,
                                    mulib_browser_eventloop_owned_run_, job))
    {
      free (job);
      return 0;
    }
  return 1;
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
  if (budget > SIZE_MAX - n)
    budget = SIZE_MAX - n;
  return n + mulib_microjs_microtask_drain (&e->microtasks, budget);
}
int
mulib_browser_eventloop_at_checkpoint (const mulib_browser_eventloop_t *e)
{
  return e && !e->tasks.draining && !e->microtasks.draining
         && e->microtasks.head == e->microtasks.len;
}
void
mulib_browser_eventloop_free (mulib_browser_eventloop_t *e)
{
  /* Neither queue may be discarded while the other is dispatching. */
  if (!e || e->tasks.draining || e->microtasks.draining)
    return;
  e->tasks.draining = e->microtasks.draining = MULIB_BROWSER_EVENTLOOP_FREEING_;
  for (size_t i = e->microtasks.head; i < e->microtasks.len; ++i)
    if (e->microtasks.jobs[i].cb == mulib_browser_eventloop_owned_run_)
      {
        mulib_browser_eventloop_owned_ *job
            = (mulib_browser_eventloop_owned_ *)e->microtasks.jobs[i].ctx;
        job->cleanup (job->ctx);
        free (job);
      }
  e->tasks.draining = e->microtasks.draining = 0;
  mulib_microjs_microtask_free (&e->tasks);
  mulib_microjs_microtask_free (&e->microtasks);
}
#endif
#endif
