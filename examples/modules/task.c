#include "../tasks-common.h"
#include "mulib-task.h"
#include <assert.h>
int main (void)
{
  mulib_task_t task;
  mulib_exec_result_t r = {0};
  int rc;
  mulib_task_init (&task); task.name = "greeting";
  task.kind = MULIB_TASK_SHELL; task.source = "printf 'hello task'";
  rc = mulib_task_run (&task, &r);
  if (rc != MULIB_EXEC_UNSUPPORTED) assert (!rc && !r.code && r.out_size == 10);
  mulib_exec_result_destroy (&r); return 0;
}
