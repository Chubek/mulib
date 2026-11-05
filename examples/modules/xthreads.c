#define MULIB_IMPL_XTHREADS
#include "mulib-xthreads.h"
#include <assert.h>
static int worker (void *arg) { return *(int *)arg + 1; }
int main (void)
{
  mulib_xthread_t thread; int input = 41, result = 0;
  assert (!mulib_xthread_create (&thread, worker, &input));
  assert (!mulib_xthread_join (thread, &result));
  assert (result == 42); return 0;
}
