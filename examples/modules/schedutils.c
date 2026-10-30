#define MULIB_IMPL_SCHEDUTILS
#include "mulib-schedutils.h"
#include <assert.h>
int
main (void)
{
  uint64_t next;
  assert (mulib_schedutils_next (100, 10, 125, &next) && next == 130);
  assert (!mulib_schedutils_next (UINT64_MAX, 1, UINT64_MAX, &next));
#if defined(__unix__) || defined(__APPLE__)
  {
    int policy, priority;
    assert (!mulib_schedutils_current (&policy, &priority));
    assert (!mulib_schedutils_yield ());
  }
#endif
  return 0;
}
