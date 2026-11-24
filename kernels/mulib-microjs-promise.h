/** @file mulib-microjs-promise.h
 * @brief Native deferred-job queues and one-shot promise-like settlement.
 * C11 single-header module. Define MULIB_IMPL_MICROJS_PROMISE in exactly one C
 * translation unit. A native job-queue substrate; not an ECMAScript Promise
 * implementation.
 */
#ifndef MULIB_MICROJS_PROMISE_H
#define MULIB_MICROJS_PROMISE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum mulib_microjs_promise_state
  {
    MULIB_MICROJS_PROMISE_PENDING = 0,
    MULIB_MICROJS_PROMISE_RESOLVED = 1,
    MULIB_MICROJS_PROMISE_REJECTED = 2
  } mulib_microjs_promise_state_t;
  typedef struct mulib_microjs_promise
  {
    mulib_microjs_promise_state_t state;
    void *value;
  } mulib_microjs_promise_t;
  typedef void (*mulib_microjs_job_fn) (void *ctx);
  typedef struct mulib_microjs_job
  {
    mulib_microjs_job_fn cb;
    void *ctx;
  } mulib_microjs_job_t;
  typedef struct mulib_microjs_microtasks
  {
    mulib_microjs_job_t *jobs;
    size_t len, cap, head, limit;
    int draining;
  } mulib_microjs_microtasks_t;
  /** @brief See manual/mulib-microjs-promise.md for this operation's contract.
   */
  int mulib_microjs_promise_settle (mulib_microjs_promise_t *p, int reject,
                                    void *value);
  /** @brief See manual/mulib-microjs-promise.md for this operation's contract.
   */
  int mulib_microjs_microtask_post (mulib_microjs_microtasks_t *q,
                                    mulib_microjs_job_fn fn, void *ctx);
  /** @brief See manual/mulib-microjs-promise.md for this operation's contract.
   */
  size_t mulib_microjs_microtask_drain (mulib_microjs_microtasks_t *q,
                                        size_t budget);
  /** @brief See manual/mulib-microjs-promise.md for this operation's contract.
   */
  void mulib_microjs_microtask_free (mulib_microjs_microtasks_t *q);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MICROJS_PROMISE_H */
#ifdef MULIB_IMPL_MICROJS_PROMISE
#ifndef MULIB_IMPL_MICROJS_PROMISE_ONCE
#define MULIB_IMPL_MICROJS_PROMISE_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_microjs_promise_settle (mulib_microjs_promise_t *p, int reject,
                              void *value)
{
  if (!p || p->state != MULIB_MICROJS_PROMISE_PENDING)
    return 0;
  p->value = value;
  p->state = reject ? MULIB_MICROJS_PROMISE_REJECTED
                    : MULIB_MICROJS_PROMISE_RESOLVED;
  return 1;
}
int
mulib_microjs_microtask_post (mulib_microjs_microtasks_t *q,
                              mulib_microjs_job_fn fn, void *ctx)
{
  if (!q || !fn || (q->limit && q->len - q->head >= q->limit))
    return 0;
  if (q->len == q->cap)
    {
      if (q->head)
        {
          memmove (q->jobs, q->jobs + q->head,
                   (q->len - q->head) * sizeof *q->jobs);
          q->len -= q->head;
          q->head = 0;
        }
      if (q->len == q->cap)
        {
          size_t c = q->cap ? q->cap * 2 : 16;
          if (c < q->cap || c > SIZE_MAX / sizeof *q->jobs)
            return 0;
          void *p = realloc (q->jobs, c * sizeof *q->jobs);
          if (!p)
            return 0;
          q->jobs = p;
          q->cap = c;
        }
    }
  q->jobs[q->len++] = (mulib_microjs_job_t){ fn, ctx };
  return 1;
}
size_t
mulib_microjs_microtask_drain (mulib_microjs_microtasks_t *q, size_t budget)
{
  if (!q || q->draining)
    return 0;
  size_t n = 0;
  q->draining = 1;
  while (n < budget && q->head < q->len)
    {
      mulib_microjs_job_t j = q->jobs[q->head++];
      j.cb (j.ctx);
      n++;
    }
  if (q->head == q->len)
    q->head = q->len = 0;
  q->draining = 0;
  return n;
}
void
mulib_microjs_microtask_free (mulib_microjs_microtasks_t *q)
{
  if (q && !q->draining)
    {
      free (q->jobs);
      memset (q, 0, sizeof *q);
    }
}
#endif
#endif
