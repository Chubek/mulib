#define MULIB_IMPL_TASKPOOL
#define MULIB_IMPL_PARALLEL
#define MULIB_IMPL_XTBB
#include "mulib-xtbb.h"
#include <assert.h>
static int sum (size_t i, void *acc, void *context)
{ (void)context; *(size_t *)acc += i; return 0; }
static int join (void *left, const void *right, void *context)
{ (void)context; *(size_t *)left += *(const size_t *)right; return 0; }
int main (void)
{
  mulib_xtbb_t *arena = NULL;
  size_t zero = 0, result = 0;
  int rc = mulib_xtbb_create (&arena, 4, 2);
  if (rc == MULIB_TASK_UNSUPPORTED) return 0;
  assert (!rc);
  assert (!mulib_xtbb_reduce (arena, 0, 100, 10, &zero, sizeof zero, &result, sum, join, NULL, NULL, NULL));
  assert (result == 4950); mulib_xtbb_destroy (arena); return 0;
}
