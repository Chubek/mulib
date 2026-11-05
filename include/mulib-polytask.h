/** @file mulib-polytask.h
 * @brief Bounded concurrent native tasks, strict JSON plans and scheduler adapter.
 * Define MULIB_IMPL_POLYTASK and dependency switches separately.
 */
#ifndef MULIB_POLYTASK_H
#define MULIB_POLYTASK_H
#include "mulib-task.h"
#include "mulib-taskpool.h"
#include "mulib-json.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_polytask mulib_polytask_t;
typedef struct mulib_polytask_plan mulib_polytask_plan_t;
typedef struct mulib_polytask_result {
  int status; /**< EXEC status, or TASK_CANCELLED if not started. */
  mulib_exec_result_t execution;
} mulib_polytask_result_t;
/** Create a persistent worker pool; *out must be NULL. */
int mulib_polytask_create (mulib_polytask_t **out, size_t workers, size_t capacity);
/** Join/free after all controller calls finish. NULL safe, not worker callable. */
void mulib_polytask_destroy (mulib_polytask_t *);
/** Synchronous batch, input-order results, independent failures continue. NULL
 * pool runs serially. Prevalidate every task before any launch; results must be
 * zero initialized. Return TASK_FAILED for any task API error/nonzero exit.
 * One controller per pool; do not call from that pool's workers. */
int mulib_polytask_run (mulib_polytask_t *, const mulib_task_t *, size_t count,
                        const mulib_task_cancel_t *, mulib_polytask_result_t *);
/** Destroy one result and reset it, NULL safe. */
void mulib_polytask_result_destroy (mulib_polytask_result_t *);
/** Run indexed callbacks and join before returning. Matches BuildGraph's
 * scheduler signature. NULL user executes serially; otherwise user is an
 * exclusively controlled PolyTask pool. Does not inspect callback results. */
int mulib_polytask_schedule (void *user, size_t count,
                             void (*callback)(size_t, void *), void *context);
/** Parse an owned inert plan: JSON array of task objects (see manual). Strict
 * fields/types, maximum 65536 tasks/array entries; no execution during parse.
 * *out must be NULL; returns EXEC_INVALID/MEMORY with JSON diagnostics. */
int mulib_polytask_parse (const char *, size_t, mulib_polytask_plan_t **out,
                         mulib_json_error_t *);
/** Borrow immutable task specifications until plan destruction. */
const mulib_task_t *mulib_polytask_tasks (const mulib_polytask_plan_t *, size_t *count);
/** Release owned JSON and arrays after execution completes. NULL safe. */
void mulib_polytask_plan_destroy (mulib_polytask_plan_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_POLYTASK) && !defined(MULIB_POLYTASK_IMPLEMENTED)
#define MULIB_POLYTASK_IMPLEMENTED
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
struct mulib_polytask { mulib_taskpool_t *pool; };
struct mulib_polytask_plan { mulib_json_t *json; mulib_task_t *tasks; size_t count; };
int mulib_polytask_create (mulib_polytask_t **out, size_t workers, size_t capacity)
{
  mulib_polytask_t *p;
  int rc;
  if (!out || *out || !workers || !capacity) return MULIB_TASK_INVALID;
  p = (mulib_polytask_t *)calloc (1, sizeof (*p));
  if (!p) return MULIB_TASK_NOMEM;
  rc = mulib_taskpool_create (&p->pool, workers, capacity);
  if (rc) { free (p); return rc; }
  *out = p; return 0;
}
void mulib_polytask_destroy (mulib_polytask_t *p)
{ if (p && !mulib_taskpool_wait (p->pool)) { mulib_taskpool_destroy (p->pool, 0); free (p); } }
typedef struct mulib_polytask_job_ {
  void (*fn)(size_t, void *); void *context; size_t index;
} mulib_polytask_job_;
static void mulib_polytask_call_ (void *v)
{ mulib_polytask_job_ *j = (mulib_polytask_job_ *)v; j->fn (j->index, j->context); }
int mulib_polytask_schedule (void *user, size_t n, void (*fn)(size_t, void *), void *context)
{
  mulib_polytask_t *p = (mulib_polytask_t *)user;
  mulib_polytask_job_ *jobs;
  size_t i;
  int rc;
  if (!fn || n > SIZE_MAX / sizeof (*jobs)) return MULIB_TASK_INVALID;
  if (!p) { for (i = 0; i < n; ++i) fn (i, context); return 0; }
  rc = mulib_taskpool_wait (p->pool);
  if (rc || !n) return rc;
  jobs = (mulib_polytask_job_ *)calloc (n, sizeof (*jobs));
  if (!jobs) return MULIB_TASK_NOMEM;
  for (i = 0; i < n; ++i) {
    jobs[i].fn = fn; jobs[i].context = context; jobs[i].index = i;
    do {
      rc = mulib_taskpool_submit (p->pool, mulib_polytask_call_, jobs + i, NULL);
      if (rc == MULIB_TASK_FULL) (void)mulib_taskpool_wait (p->pool);
    } while (rc == MULIB_TASK_FULL);
    if (rc) break;
  }
  (void)mulib_taskpool_wait (p->pool); free (jobs); return rc;
}
typedef struct mulib_polytask_batch_ {
  const mulib_task_t *tasks; mulib_polytask_result_t *results;
  const mulib_task_cancel_t *cancel;
} mulib_polytask_batch_;
static void mulib_polytask_one_ (size_t i, void *v)
{
  mulib_polytask_batch_ *b = (mulib_polytask_batch_ *)v;
  b->results[i].status = mulib_task_cancel_requested (b->cancel) ? MULIB_TASK_CANCELLED
    : mulib_task_run (b->tasks + i, &b->results[i].execution);
}
int mulib_polytask_run (mulib_polytask_t *p, const mulib_task_t *tasks, size_t n,
                       const mulib_task_cancel_t *cancel, mulib_polytask_result_t *results)
{
  mulib_polytask_batch_ b;
  size_t i;
  int rc;
  if ((n && (!tasks || !results)) || n > SIZE_MAX / sizeof (*results)) return MULIB_TASK_INVALID;
  for (i = 0; i < n; ++i)
    if (mulib_task_validate (tasks + i) || results[i].execution.out || results[i].execution.err
        || results[i].execution.stages) return MULIB_TASK_INVALID;
  for (i = 0; i < n; ++i) results[i].status = MULIB_TASK_CANCELLED;
  b.tasks = tasks; b.results = results; b.cancel = cancel;
  rc = mulib_polytask_schedule (p, n, mulib_polytask_one_, &b);
  if (rc) return rc;
  for (i = 0; i < n; ++i) if (results[i].status || results[i].execution.code) return MULIB_TASK_FAILED;
  return 0;
}
void mulib_polytask_result_destroy (mulib_polytask_result_t *r)
{ if (r) { mulib_exec_result_destroy (&r->execution); r->status = 0; } }
static const char *mulib_polytask_string_ (const mulib_json_t *v)
{
  size_t n;
  const char *s = mulib_json_text (v, &n);
  return s && !memchr (s, 0, n) ? s : NULL;
}
static int mulib_polytask_array_ (const mulib_json_t *v, const char *const **out, size_t *n)
{
  const char **a;
  size_t i;
  if (mulib_json_typeof (v) != MULIB_JSON_ARRAY || (*n = mulib_json_size (v)) > 65536) return MULIB_EXEC_INVALID;
  a = (const char **)calloc (*n + 1, sizeof (*a));
  if (!a) return MULIB_EXEC_MEMORY;
  for (i = 0; i < *n; ++i) if (!(a[i] = mulib_polytask_string_ (mulib_json_at (v, i)))) {
    free (a); return MULIB_EXEC_INVALID;
  }
  *out = a; return 0;
}
int mulib_polytask_parse (const char *text, size_t length, mulib_polytask_plan_t **out,
                         mulib_json_error_t *error)
{
  mulib_polytask_plan_t *p;
  size_t i, j;
  int rc = MULIB_EXEC_INVALID;
  if (error) memset (error, 0, sizeof (*error));
  if (!out || *out || (!text && length)) return rc;
  p = (mulib_polytask_plan_t *)calloc (1, sizeof (*p));
  if (!p) return MULIB_EXEC_MEMORY;
  p->json = mulib_json_parse (text ? text : "", length, error);
  if (!p->json) { mulib_polytask_plan_destroy (p); return rc; }
  if (mulib_json_typeof (p->json) != MULIB_JSON_ARRAY || (p->count = mulib_json_size (p->json)) > 65536) goto fail;
  p->tasks = (mulib_task_t *)calloc (p->count ? p->count : 1, sizeof (*p->tasks));
  if (!p->tasks) { rc = MULIB_EXEC_MEMORY; goto fail; }
  for (i = 0; i < p->count; ++i) {
    mulib_task_t *t = p->tasks + i;
    mulib_json_t *o = mulib_json_at (p->json, i);
    unsigned execution = 0;
    mulib_task_init (t);
    if (mulib_json_typeof (o) != MULIB_JSON_OBJECT) goto fail;
    for (j = 0; j < mulib_json_size (o); ++j) {
      size_t kn;
      const char *key = mulib_json_key (o, j, &kn);
      mulib_json_t *v = mulib_json_at (o, j);
      const char *s = mulib_polytask_string_ (v);
      if (memchr (key, 0, kn)) goto fail;
      if (!strcmp (key, "name")) { if (!s) goto fail; t->name = s; }
      else if (!strcmp (key, "file") || !strcmp (key, "shell")) {
        if (!s) goto fail;
        t->kind = !strcmp (key, "file") ? MULIB_TASK_FILE : MULIB_TASK_SHELL;
        t->source = s; ++execution;
      } else if (!strcmp (key, "argv")) {
        rc = mulib_polytask_array_ (v, &t->command.argv, &t->command.argc);
        if (rc) goto fail;
        ++execution;
      } else if (!strcmp (key, "env") || !strcmp (key, "inputs") || !strcmp (key, "outputs")) {
        if (!strcmp (key, "env")) rc = mulib_polytask_array_ (v, &t->command.env, &t->command.envc);
        else if (!strcmp (key, "inputs")) rc = mulib_polytask_array_ (v, &t->inputs, &t->input_count);
        else rc = mulib_polytask_array_ (v, &t->outputs, &t->output_count);
        if (rc) goto fail;
      } else if (!strcmp (key, "cwd")) { if (!s) goto fail; t->command.cwd = s; }
      else if (!strcmp (key, "timeout_ms") || !strcmp (key, "max_output")) {
        double d = mulib_json_numeric (v);
        if (mulib_json_typeof (v) != MULIB_JSON_NUMBER || d < 0 || d > UINT_MAX || d != (unsigned)d) goto fail;
        if (!strcmp (key, "timeout_ms")) t->options.timeout_ms = (unsigned)d;
        else t->options.max_output = (size_t)d;
      } else goto fail;
      rc = MULIB_EXEC_INVALID;
    }
    if (execution != 1 || mulib_task_validate (t)) goto fail;
    for (j = 0; j < i; ++j) if (!strcmp (t->name, p->tasks[j].name)) goto fail;
  }
  *out = p; return 0;
fail:
  if (error && !error->message) error->message = "invalid PolyTask schema";
  mulib_polytask_plan_destroy (p); return rc;
}
const mulib_task_t *mulib_polytask_tasks (const mulib_polytask_plan_t *p, size_t *n)
{ if (n) *n = p ? p->count : 0; return p ? p->tasks : NULL; }
void mulib_polytask_plan_destroy (mulib_polytask_plan_t *p)
{
  mulib_task_t defaults;
  size_t i;
  if (!p) return;
  mulib_task_init (&defaults);
  for (i = 0; p->tasks && i < p->count; ++i) {
    free ((void *)p->tasks[i].command.argv);
    /* task_init's shared empty environment is not heap storage. */
    if (p->tasks[i].command.env != defaults.command.env) free ((void *)p->tasks[i].command.env);
    free ((void *)p->tasks[i].inputs); free ((void *)p->tasks[i].outputs);
  }
  free (p->tasks); mulib_json_destroy (p->json); free (p);
}
#endif
