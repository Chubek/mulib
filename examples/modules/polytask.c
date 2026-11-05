#include "../tasks-common.h"
#include "mulib-polytask.h"
#include <assert.h>
#include <string.h>
int main (void)
{
  const char json[] = "[{\"name\":\"one\",\"argv\":[\"printf\",\"one\"]},"
                      "{\"name\":\"two\",\"shell\":\"printf two\"}]";
  mulib_polytask_t *pool = NULL;
  mulib_polytask_plan_t *plan = NULL;
  mulib_polytask_result_t results[2] = {{0}};
  const mulib_task_t *tasks;
  size_t n, i;
  int rc = mulib_polytask_create (&pool, 2, 2);
  if (rc == MULIB_TASK_UNSUPPORTED) return 0;
  assert (!rc && !mulib_polytask_parse (json, strlen (json), &plan, NULL));
  tasks = mulib_polytask_tasks (plan, &n);
  assert (n == 2 && !mulib_polytask_run (pool, tasks, n, NULL, results));
  for (i = 0; i < n; ++i) mulib_polytask_result_destroy (results + i);
  mulib_polytask_plan_destroy (plan); mulib_polytask_destroy (pool); return 0;
}
