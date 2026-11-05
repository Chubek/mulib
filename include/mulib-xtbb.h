/** @file mulib-xtbb.h
 * @brief C11 task arenas, parallel algorithms and deterministic reductions.
 * Focused OneTBB-inspired API, not its ABI or work-stealing scheduler.
 */
#ifndef MULIB_XTBB_H
#define MULIB_XTBB_H
#include "mulib-parallel.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_xtbb mulib_xtbb_t;
/** Each chunk initializes one private fixed-size accumulator from identity.
 * body folds index into accumulator. join merges right into left, serially in
 * ascending chunk order. Accumulators must be plain byte-copyable values. */
typedef int (*mulib_xtbb_reduce_fn) (size_t index, void *accumulator, void *context);
typedef int (*mulib_xtbb_join_fn) (void *left, const void *right, void *context);
typedef struct mulib_xtbb_job {
  int (*function)(void *);
  void *context;
} mulib_xtbb_job_t;
/** Create bounded arena; *out must be NULL. One controlling caller per arena. */
int mulib_xtbb_create (mulib_xtbb_t **out, size_t workers, size_t capacity);
/** Join/destroy arena; NULL safe. Not callable inside its callbacks. */
void mulib_xtbb_destroy (mulib_xtbb_t *);
/** Parallel [begin,end), with cooperative cancellation and original error code.
 * Do not call synchronous arena operations from that arena's callbacks. */
int mulib_xtbb_for (mulib_xtbb_t *, size_t begin, size_t end, size_t grain,
                    mulib_parallel_fn, void *, const mulib_task_cancel_t *, int *error);
/** Map fixed-stride slots; same overlap/ownership rules as mulib_parallel_map. */
int mulib_xtbb_map (mulib_xtbb_t *, const void *, size_t input_stride, void *,
                    size_t output_stride, size_t count, size_t grain,
                    mulib_parallel_map_fn, void *, const mulib_task_cancel_t *, int *error);
/** Fork/join independent borrowed callbacks. Failure cooperatively stops jobs
 * not yet started; already running jobs finish. All callbacks are joined. */
int mulib_xtbb_invoke (mulib_xtbb_t *, const mulib_xtbb_job_t *, size_t count,
                       const mulib_task_cancel_t *, int *error);
/** Parallel reduction. Output is copied only on success; empty range copies
 * identity. join order is deterministic for a fixed grain, not necessarily
 * equivalent to a serial fold for non-associative operations. */
int mulib_xtbb_reduce (mulib_xtbb_t *, size_t begin, size_t end, size_t grain,
                       const void *identity, size_t size, void *output,
                       mulib_xtbb_reduce_fn, mulib_xtbb_join_fn, void *context,
                       const mulib_task_cancel_t *, int *error);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_XTBB) && !defined(MULIB_XTBB_IMPLEMENTED)
#define MULIB_XTBB_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
struct mulib_xtbb { mulib_taskpool_t *pool; };
int mulib_xtbb_create (mulib_xtbb_t **out, size_t workers, size_t capacity)
{
  mulib_xtbb_t *p;
  int rc;
  if (!out || *out) return MULIB_TASK_INVALID;
  p = (mulib_xtbb_t *)calloc (1, sizeof (*p));
  if (!p) return MULIB_TASK_NOMEM;
  rc = mulib_taskpool_create (&p->pool, workers, capacity);
  if (rc) { free (p); return rc; }
  *out = p; return 0;
}
void mulib_xtbb_destroy (mulib_xtbb_t *p)
{ if (p && !mulib_taskpool_wait (p->pool)) { mulib_taskpool_destroy (p->pool, 0); free (p); } }
int mulib_xtbb_for (mulib_xtbb_t *p, size_t begin, size_t end, size_t grain,
                    mulib_parallel_fn fn, void *context, const mulib_task_cancel_t *cancel, int *error)
{ return mulib_parallel_for (p ? p->pool : NULL, begin, end, grain, fn, context, cancel, error); }
int mulib_xtbb_map (mulib_xtbb_t *p, const void *in, size_t is, void *out, size_t os,
                    size_t n, size_t grain, mulib_parallel_map_fn fn, void *context,
                    const mulib_task_cancel_t *cancel, int *error)
{ return mulib_parallel_map (p ? p->pool : NULL, in, is, out, os, n, grain, fn, context, cancel, error); }
static int mulib_xtbb_invoke_one_ (size_t i, void *context)
{
  const mulib_xtbb_job_t *jobs = (const mulib_xtbb_job_t *)context;
  return jobs[i].function (jobs[i].context);
}
int mulib_xtbb_invoke (mulib_xtbb_t *p, const mulib_xtbb_job_t *jobs, size_t n,
                       const mulib_task_cancel_t *cancel, int *error)
{
  size_t i;
  if (error) *error = 0;
  if ((!jobs && n) || n > SIZE_MAX / sizeof (*jobs)) return MULIB_TASK_INVALID;
  for (i = 0; i < n; ++i) if (!jobs[i].function) return MULIB_TASK_INVALID;
  return mulib_xtbb_for (p, 0, n, 1, mulib_xtbb_invoke_one_, (void *)jobs, cancel, error);
}
typedef struct mulib_xtbb_reduction_ {
  size_t begin, end, grain, size;
  unsigned char *partials;
  mulib_xtbb_reduce_fn fn; void *context;
  const mulib_task_cancel_t *cancel;
} mulib_xtbb_reduction_;
static int mulib_xtbb_chunk_ (size_t chunk, void *context)
{
  mulib_xtbb_reduction_ *r = (mulib_xtbb_reduction_ *)context;
  size_t i = r->begin + chunk * r->grain;
  size_t end = r->end - i < r->grain ? r->end : i + r->grain;
  for (; i < end; ++i) {
    int rc;
    if (mulib_task_cancel_requested (r->cancel)) return 0;
    rc = r->fn (i, r->partials + chunk * r->size, r->context);
    if (rc) return rc;
  }
  return 0;
}
int mulib_xtbb_reduce (mulib_xtbb_t *p, size_t begin, size_t end, size_t grain,
                       const void *identity, size_t size, void *output,
                       mulib_xtbb_reduce_fn fn, mulib_xtbb_join_fn join, void *context,
                       const mulib_task_cancel_t *cancel, int *error)
{
  mulib_xtbb_reduction_ r;
  unsigned char *acc;
  size_t n, i;
  int rc;
  if (error) *error = 0;
  if (!p || !grain || end < begin || !size || !identity || !output || !fn || !join) return MULIB_TASK_INVALID;
  rc = mulib_taskpool_wait (p->pool);
  if (rc) return rc;
  if (mulib_task_cancel_requested (cancel)) return MULIB_TASK_CANCELLED;
  n = (end - begin) / grain + ((end - begin) % grain != 0);
  if (n > SIZE_MAX / size - 1) return MULIB_TASK_INVALID;
  r.partials = (unsigned char *)malloc ((n + 1) * size);
  if (!r.partials) return MULIB_TASK_NOMEM;
  for (i = 0; i <= n; ++i) memcpy (r.partials + i * size, identity, size);
  acc = r.partials + n * size;
  r.begin = begin; r.end = end; r.grain = grain; r.size = size; r.fn = fn; r.context = context; r.cancel = cancel;
  rc = mulib_xtbb_for (p, 0, n, 1, mulib_xtbb_chunk_, &r, cancel, error);
  for (i = 0; !rc && i < n; ++i) {
    if (mulib_task_cancel_requested (cancel)) { rc = MULIB_TASK_CANCELLED; break; }
    rc = join (acc, r.partials + i * size, context);
    if (rc) { if (error) *error = rc; rc = MULIB_TASK_FAILED; }
  }
  if (!rc && mulib_task_cancel_requested (cancel)) rc = MULIB_TASK_CANCELLED;
  if (!rc) memcpy (output, acc, size);
  free (r.partials); return rc;
}
#endif
