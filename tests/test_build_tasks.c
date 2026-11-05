#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static long allocation_budget = -1;
static void *test_allocate (size_t n)
{
  if (!allocation_budget) return NULL;
  if (allocation_budget > 0) --allocation_budget;
  return malloc (n);
}
static void *test_callocate (size_t n, size_t size)
{
  void *p;
  if (size && n > SIZE_MAX / size) return NULL;
  p = test_allocate (n * size);
  if (p) memset (p, 0, n * size);
  return p;
}
static void *test_reallocate (void *p, size_t n)
{
  if (!allocation_budget) return NULL;
  if (allocation_budget > 0) --allocation_budget;
  return realloc (p, n);
}
#define malloc test_allocate
#define calloc test_callocate
#define realloc test_reallocate
#include "../examples/tasks-common.h"
#define MULIB_IMPL_ARTIFACT
#define MULIB_IMPL_BUILDGRAPH
#define MULIB_IMPL_VCDIFF
#define MULIB_IMPL_BUILDVM
#define MULIB_IMPL_PARALLEL
#define MULIB_IMPL_XTBB
#include "mulib-polytask.h"
#include "mulib-buildvm.h"
#include "mulib-xtbb.h"
#undef malloc
#undef calloc
#undef realloc
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#endif
static void archives (void)
{
  mulib_artifact_file_t files[2] = {{"dir/a", "a\0b", 3, 42, 0644, MULIB_ARTIFACT_FILE},
                                   {"dir/b.diff", "delta", 5, 0, 0600, MULIB_ARTIFACT_DIFF}};
  mulib_artifact_file_t f;
  mulib_artifact_t *a = mulib_artifact_pack ("bundle", files, 2), *b;
  const mulib_artifact_desc_t *d;
  unsigned char *copy;
  size_t n, i;
  assert (a && !mulib_artifact_file_count (a, &n) && n == 2);
  assert (!mulib_artifact_file_at (a, 0, &f));
  assert (!strcmp (f.path, "dir/a") && f.mtime == 42 && f.mode == 0644 && !memcmp (f.data, "a\0b", 3));
  assert (!mulib_artifact_file_at (a, 1, &f) && f.kind == MULIB_ARTIFACT_DIFF);
  assert (mulib_artifact_file_at (a, 2, &f) == -1);
  d = mulib_artifact_view (a); b = mulib_artifact_unpack ("copy", d->data, d->length);
  assert (b && !memcmp (d->data, mulib_artifact_view (b)->data, d->length));
  copy = (unsigned char *)malloc (d->length); assert (copy);
  memcpy (copy, d->data, d->length); copy[24] = 0;
  assert (!mulib_artifact_unpack ("bad", copy, d->length));
  for (i = 0; i < d->length; ++i) assert (!mulib_artifact_unpack ("truncated", d->data, i));
  free (copy); mulib_artifact_release (a); mulib_artifact_release (b);
  files[1].path = files[0].path; assert (!mulib_artifact_pack ("duplicate", files, 2));
  files[0].path = "../escape"; assert (!mulib_artifact_pack ("escape", files, 1));
  a = mulib_artifact_pack ("empty", NULL, 0); assert (a && !mulib_artifact_file_count (a, &n) && !n);
  mulib_artifact_release (a);
}
static void plans (mulib_polytask_t *pool)
{
  const char *bad[] = {"", "{}", "[{}]", "[{\"name\":\"x\",\"shell\":3}]",
    "[{\"name\":\"x\",\"shell\":\"\",\"argv\":[\"true\"]}]",
    "[{\"name\":\"x\",\"argv\":[]}]", "[{\"name\":\"x\",\"shell\":\"\",\"wat\":0}]",
    "[{\"name\":\"x\",\"shell\":\"\",\"timeout_ms\":1.5}]",
    "[{\"name\":\"x\",\"shell\":\"\",\"inputs\":[\"../bad\"]}]",
    "[{\"name\":\"x\",\"shell\":\"\",\"env\":[\"bad\"]}]",
    "[{\"name\":\"x\",\"shell\":\"\\u0000\"}]",
    "[{\"name\":\"x\",\"shell\":\"\"},{\"name\":\"x\",\"shell\":\"\"}]"};
  const char json[] = "[{\"name\":\"a\",\"shell\":\"printf a\"},"
    "{\"name\":\"b\",\"argv\":[\"printf\",\"b\"]},"
    "{\"name\":\"failure\",\"shell\":\"printf error >&2; exit 7\"}]";
  mulib_polytask_plan_t *plan = NULL;
  mulib_polytask_result_t r[3] = {{0}};
  mulib_task_cancel_t *cancel;
  const mulib_task_t *tasks;
  mulib_json_error_t error;
  size_t i, n;
  for (i = 0; i < sizeof bad / sizeof *bad; ++i) {
    assert (mulib_polytask_parse (bad[i], strlen (bad[i]), &plan, &error) == MULIB_EXEC_INVALID);
    assert (!plan && error.message);
  }
  assert (!mulib_polytask_parse ("[]", 2, &plan, NULL));
  assert (mulib_polytask_tasks (plan, &n) && !n);
  mulib_polytask_plan_destroy (plan); plan = NULL;
  assert (!mulib_polytask_parse (json, sizeof json - 1, &plan, NULL));
  tasks = mulib_polytask_tasks (plan, &n); assert (n == 3);
  assert (mulib_polytask_run (pool, tasks, n, NULL, r) == MULIB_TASK_FAILED);
  assert (!r[0].status && !strcmp (r[0].execution.out, "a"));
  assert (!r[1].status && !strcmp (r[1].execution.out, "b"));
  assert (!r[2].status && r[2].execution.code == 7 && !strcmp (r[2].execution.err, "error"));
  for (i = 0; i < n; ++i) mulib_polytask_result_destroy (r + i);
  cancel = mulib_task_cancel_create (); assert (cancel); mulib_task_cancel_request (cancel);
  assert (mulib_polytask_run (pool, tasks, n, cancel, r) == MULIB_TASK_FAILED);
  for (i = 0; i < n; ++i) assert (r[i].status == MULIB_TASK_CANCELLED && !r[i].execution.stages);
  mulib_task_cancel_destroy (cancel); mulib_polytask_plan_destroy (plan);
}
static void allocation_failures (void)
{
  const char json[] = "[{\"name\":\"test\",\"argv\":[\"true\"],\"env\":[\"X=y\"],"
    "\"inputs\":[\"input\"],\"outputs\":[\"output\"]}]";
  mulib_polytask_plan_t *p = NULL;
  mulib_buildvm_instruction_t code[2] = {{0}};
  mulib_buildvm_program_t program = {code, 2, 1, 64};
  mulib_buildgraph_t *g;
  mulib_buildgraph_run_t *r = NULL;
  size_t task, output;
  long i;
  int succeeded = 0;
  for (i = 0; i < 512; ++i) {
    int rc;
    allocation_budget = i;
    rc = mulib_polytask_parse (json, sizeof json - 1, &p, NULL);
    allocation_budget = -1;
    if (!rc) { mulib_polytask_plan_destroy (p); p = NULL; succeeded = 1; break; }
    assert (!p && (rc == MULIB_EXEC_INVALID || rc == MULIB_EXEC_MEMORY));
  }
  assert (succeeded);
  code[0].opcode = MULIB_BUILDVM_LITERAL; code[0].data = "data"; code[0].length = 4;
  code[1].opcode = MULIB_BUILDVM_EMIT;
  g = mulib_buildgraph_create (2, 1); assert (g);
  assert (!mulib_buildvm_task (g, "task", &program, &task));
  assert (!mulib_buildgraph_artifact (g, "out", NULL, &output));
  assert (!mulib_buildgraph_depend (g, task, output));
  succeeded = 0;
  for (i = 0; i < 64; ++i) {
    int rc;
    allocation_budget = i;
    rc = mulib_buildgraph_build (g, NULL, 0, NULL, NULL, &r);
    allocation_budget = -1;
    if (!rc) succeeded = 1;
    else if (rc == MULIB_BUILDGRAPH_MEMORY) assert (!r);
    else assert (rc == MULIB_BUILDGRAPH_FAILED && r && !mulib_buildgraph_value (r, output));
    mulib_buildgraph_run_destroy (r); r = NULL;
    if (succeeded) break;
  }
  assert (succeeded); mulib_buildgraph_destroy (g);
}
static void native_tasks (void)
{
  mulib_task_t task;
  mulib_exec_result_t r = {0};
  mulib_task_init (&task); task.name = "shell"; task.kind = MULIB_TASK_SHELL;
  task.source = "printf '%s' \"${MULIB_TASK_SECRET-unset}\"";
  assert (!mulib_task_run (&task, &r) && !strcmp (r.out, "unset"));
  mulib_exec_result_destroy (&r);
  task.source = "exec sleep 1"; task.options.timeout_ms = 10;
  assert (mulib_task_run (&task, &r) == MULIB_EXEC_TIMEOUT); mulib_exec_result_destroy (&r);
  task.source = "printf abcdef"; task.options.timeout_ms = 1000; task.options.max_output = 2;
  assert (mulib_task_run (&task, &r) == MULIB_EXEC_LIMIT); mulib_exec_result_destroy (&r);
  task.kind = MULIB_TASK_FILE; task.source = "/definitely/not/a/mulib-task";
  assert (mulib_task_run (&task, &r) == MULIB_EXEC_SYSTEM); mulib_exec_result_destroy (&r);
#if defined(__unix__) || defined(__APPLE__)
  {
    FILE *f = fopen ("task-script", "wb"); assert (f);
    assert (fputs ("#!/bin/sh\nprintf shebang", f) >= 0 && !fclose (f));
    assert (!chmod ("task-script", 0700)); task.source = "./task-script"; task.options.max_output = 100;
    assert (!mulib_task_run (&task, &r) && !strcmp (r.out, "shebang"));
    mulib_exec_result_destroy (&r); assert (!remove ("task-script"));
  }
#endif
}
static void concurrent_native_tasks (mulib_polytask_t *pool)
{
  mulib_task_t tasks[2];
  mulib_polytask_result_t results[2] = {{0}};
  size_t i;
  (void)remove ("task-ready-a"); (void)remove ("task-ready-b");
  for (i = 0; i < 2; ++i) {
    mulib_task_init (tasks + i); tasks[i].kind = MULIB_TASK_SHELL;
    tasks[i].options.timeout_ms = 5000;
  }
  tasks[0].name = "a"; tasks[1].name = "b";
  tasks[0].source = ": > task-ready-a; while [ ! -f task-ready-b ]; do :; done; printf a";
  tasks[1].source = ": > task-ready-b; while [ ! -f task-ready-a ]; do :; done; printf b";
  assert (!mulib_polytask_run (pool, tasks, 2, NULL, results));
  assert (!strcmp (results[0].execution.out, "a") && !strcmp (results[1].execution.out, "b"));
  for (i = 0; i < 2; ++i) mulib_polytask_result_destroy (results + i);
  assert (!remove ("task-ready-a") && !remove ("task-ready-b"));
}
static atomic_int concurrent, peak, arrived;
static int concurrent_task (void *context, mulib_buildgraph_step_t *step)
{
  const mulib_buildgraph_binding_t *in;
  size_t n;
  int active, old;
  (void)context;
  active = atomic_fetch_add (&concurrent, 1) + 1; old = atomic_load (&peak);
  while (old < active && !atomic_compare_exchange_weak (&peak, &old, active)) {}
  atomic_fetch_add (&arrived, 1);
  /* Bounded rendezvous: failure is an assertion, never an infinite test hang. */
  { size_t spin; for (spin = 0; spin < 100000000 && atomic_load (&arrived) < 2; ++spin) {} }
  assert (atomic_load (&arrived) == 2);
  in = mulib_buildgraph_inputs (step, &n); assert (n == 1);
  assert (!mulib_buildgraph_output (step, 0, (mulib_artifact_t *)in[0].artifact));
  atomic_fetch_sub (&concurrent, 1); return 0;
}
static void graphs (mulib_polytask_t *pool)
{
  mulib_buildvm_instruction_t code[7] = {{0}};
  mulib_buildvm_program_t program = {code, 7, 4, 1024};
  mulib_artifact_desc_t d = {0};
  mulib_artifact_t *a;
  mulib_buildgraph_t *g;
  mulib_buildgraph_run_t *run = NULL;
  mulib_buildgraph_state_t state;
  size_t input, task, out, t1, t2, o1, o2;
  int result;
  d.id = "source"; d.type = "file"; d.data = "old"; d.length = 3;
  a = mulib_artifact_create (&d); g = mulib_buildgraph_create (7, 6); assert (a && g);
  code[0].opcode = MULIB_BUILDVM_INPUT;
  code[1].opcode = MULIB_BUILDVM_LITERAL; code[1].dst = 1; code[1].data = "new\0bytes"; code[1].length = 9;
  code[2].opcode = MULIB_BUILDVM_DIFF; code[2].dst = 2; code[2].b = 1;
  code[3].opcode = MULIB_BUILDVM_PATCH; code[3].dst = 3; code[3].b = 2; code[3].length = 9;
  code[4].opcode = MULIB_BUILDVM_SLICE; code[4].dst = 0; code[4].a = 3; code[4].length = 3;
  code[5].opcode = MULIB_BUILDVM_CONCAT; code[5].dst = 0; code[5].b = 1;
  code[6].opcode = MULIB_BUILDVM_EMIT;
  assert (!mulib_buildgraph_artifact (g, "input", a, &input));
  assert (!mulib_buildvm_task (g, "vm", &program, &task));
  assert (!mulib_buildgraph_artifact (g, "output", NULL, &out));
  assert (!mulib_buildgraph_depend (g, input, task) && !mulib_buildgraph_depend (g, task, out));
  assert (!mulib_buildgraph_task (g, "one", concurrent_task, NULL, &t1));
  assert (!mulib_buildgraph_task (g, "two", concurrent_task, NULL, &t2));
  assert (!mulib_buildgraph_artifact (g, "o1", NULL, &o1) && !mulib_buildgraph_artifact (g, "o2", NULL, &o2));
  assert (!mulib_buildgraph_depend (g, input, t1) && !mulib_buildgraph_depend (g, input, t2));
  assert (!mulib_buildgraph_depend (g, t1, o1) && !mulib_buildgraph_depend (g, t2, o2));
  assert (!mulib_buildgraph_build (g, NULL, 0, mulib_polytask_schedule, pool, &run));
  assert (atomic_load (&peak) >= 2);
  assert (mulib_artifact_view (mulib_buildgraph_value (run, out))->length == 12);
  assert (!memcmp (mulib_artifact_view (mulib_buildgraph_value (run, out))->data, "newnew\0bytes", 12));
  mulib_buildgraph_run_destroy (run); run = NULL;
  /* Target selection excludes the native callback branches; serial VM remains hermetic. */
  assert (!mulib_buildgraph_build (g, &out, 1, NULL, NULL, &run));
  mulib_buildgraph_run_destroy (run); run = NULL;
  program.max_bytes = 1;
  assert (mulib_buildgraph_build (g, &out, 1, NULL, NULL, &run) == MULIB_BUILDGRAPH_FAILED);
  assert (!mulib_buildgraph_result (run, task, &state, &result) && state == MULIB_BUILDGRAPH_ERRORED && result == MULIB_BUILDVM_LIMIT);
  assert (!mulib_buildgraph_value (run, out)); mulib_buildgraph_run_destroy (run); run = NULL;
  program.max_bytes = 1024; code[6].a = 20;
  assert (mulib_buildvm_validate (&program) == MULIB_BUILDVM_INVALID);
  code[6].a = 0; code[4].offset = 100;
  assert (mulib_buildgraph_build (g, &out, 1, NULL, NULL, &run) == MULIB_BUILDGRAPH_FAILED);
  mulib_buildgraph_run_destroy (run); run = NULL;
  code[4].opcode = MULIB_BUILDVM_EMIT; code[4].a = 3; code[4].b = 0;
  code[5].opcode = MULIB_BUILDVM_SLICE; code[5].a = 3; code[5].offset = 100; code[5].length = 1;
  assert (mulib_buildgraph_build (g, &out, 1, NULL, NULL, &run) == MULIB_BUILDGRAPH_FAILED);
  assert (!mulib_buildgraph_value (run, out)); /* earlier EMIT rolls back */
  mulib_buildgraph_run_destroy (run); mulib_buildgraph_destroy (g); mulib_artifact_release (a);
}
static int sum (size_t i, void *acc, void *context)
{ if (context && i == 5) return 91; *(size_t *)acc += i; return 0; }
static int join (void *a, const void *b, void *context)
{ (void)context; *(size_t *)a += *(const size_t *)b; return 0; }
static int invoke (void *context) { ++*(size_t *)context; return 0; }
static int map_one (size_t i, const void *in, void *out, void *context)
{ (void)i; (void)context; *(size_t *)out = *(const size_t *)in * 2; return 0; }
static void reductions (void)
{
  mulib_xtbb_t *arena = NULL;
  size_t zero = 0, value = 42;
  int error;
  assert (!mulib_xtbb_create (&arena, 3, 1));
  assert (!mulib_xtbb_reduce (arena, 0, 100, 7, &zero, sizeof zero, &value, sum, join, NULL, NULL, &error));
  assert (value == 4950 && !error);
  assert (!mulib_xtbb_reduce (arena, 2, 2, 1, &zero, sizeof zero, &value, sum, join, NULL, NULL, &error) && !value);
  value = 42;
  assert (mulib_xtbb_reduce (arena, 0, 10, 2, &zero, sizeof zero, &value, sum, join, &error, NULL, &error) == MULIB_TASK_FAILED);
  assert (value == 42 && error == 91);
  assert (mulib_xtbb_reduce (arena, 0, SIZE_MAX, 1, &zero, sizeof zero, &value, sum, join, NULL, NULL, NULL) == MULIB_TASK_INVALID);
  {
    size_t a = 0, b = 0, in[2] = {2, 3}, out[2] = {0};
    mulib_xtbb_job_t jobs[2] = {{invoke, &a}, {invoke, &b}};
    assert (!mulib_xtbb_invoke (arena, jobs, 2, NULL, &error) && a == 1 && b == 1);
    assert (!mulib_xtbb_map (arena, in, sizeof (size_t), out, sizeof (size_t), 2, 1, map_one, NULL, NULL, &error));
    assert (out[0] == 4 && out[1] == 6);
    jobs[1].function = NULL;
    assert (mulib_xtbb_invoke (arena, jobs, 2, NULL, &error) == MULIB_TASK_INVALID && a == 1);
  }
  mulib_xtbb_destroy (arena);
}
int main (void)
{
  mulib_polytask_t *pool = NULL;
  int rc;
  archives (); allocation_failures ();
  rc = mulib_polytask_create (&pool, 3, 4);
  if (rc == MULIB_TASK_UNSUPPORTED) return 0;
  assert (!rc); plans (pool); native_tasks (); concurrent_native_tasks (pool); graphs (pool); reductions ();
  mulib_polytask_destroy (pool); return 0;
}
