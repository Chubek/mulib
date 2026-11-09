#define MULIB_IMPL_BROWSERSNAPSHOT
#include "mulib-browsersnapshot.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  mulib_browsersnapshot_t s = { 0 };
  assert (mulib_browsersnapshot_create (&s, "abc", 3));
  assert (s.size == 3);
  assert (s.hash != 0);
  mulib_browsersnapshot_free (&s);
  return 0;
}
