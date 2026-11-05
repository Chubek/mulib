#define MULIB_IMPL_XTHREADS
#define MULIB_IMPL_FIBER
#define MULIB_IMPL_CONCURRENCY_PRIM
#include "mulib-concurrency-prim.h"
#include <assert.h>
typedef struct state { mulib_waitgroup_t *group; mulib_mutex_t *mutex; int value; } state;
static void worker (void *arg)
{
  state *s = (state *)arg;
  assert (!mulib_mutex_lock (s->mutex, MULIB_XWAIT_FOREVER));
  ++s->value; mulib_fiber_yield ();
  assert (!mulib_mutex_unlock (s->mutex));
  assert (!mulib_waitgroup_done (s->group));
}
int main (void)
{
  state s; mulib_fiber_scheduler_t *scheduler;
  s.value = 0;
  assert (!mulib_waitgroup_create (&s.group)); assert (!mulib_mutex_create (&s.mutex));
  assert (!mulib_waitgroup_add (s.group, 2));
  assert (!mulib_fiber_scheduler_create (&scheduler));
  assert (!mulib_fiber_spawn (scheduler, worker, &s, 0, NULL));
  assert (!mulib_fiber_spawn (scheduler, worker, &s, 0, NULL));
  assert (!mulib_fiber_run (scheduler));
  assert (!mulib_waitgroup_wait (s.group, 0) && s.value == 2);
  assert (!mulib_fiber_scheduler_destroy (scheduler));
  assert (!mulib_waitgroup_destroy (s.group)); assert (!mulib_mutex_destroy (s.mutex));
  return 0;
}
