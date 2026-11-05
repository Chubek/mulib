#define MULIB_IMPL_XTHREADS
#define MULIB_IMPL_FIBER
#include "mulib-fiber.h"
#include <assert.h>
static void worker (void *arg)
{
  int *count = (int *)arg;
  ++*count; mulib_fiber_yield (); ++*count;
}
int main (void)
{
  mulib_fiber_scheduler_t *scheduler; int count = 0;
  assert (!mulib_fiber_scheduler_create (&scheduler));
  assert (!mulib_fiber_spawn (scheduler, worker, &count, 0, NULL));
  assert (!mulib_fiber_spawn (scheduler, worker, &count, 0, NULL));
  assert (!mulib_fiber_run (scheduler));
  assert (count == 4);
  assert (!mulib_fiber_scheduler_destroy (scheduler)); return 0;
}
