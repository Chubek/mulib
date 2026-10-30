#define MULIB_IMPL_INITPROC
#include "mulib-initproc.h"
#include <assert.h>
int
main (void)
{
  int pid, status;
  assert (mulib_initproc_reap (0, 1, &pid, &status) < 0);
#if defined(__unix__) || defined(__APPLE__)
  assert (mulib_initproc_reap (-1, 1, &pid, &status)
          == 2); /* This process has no children. */
#endif
  return 0;
}
