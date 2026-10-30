#define MULIB_IMPL_OBJFILEPOSINDEP
#include "mulib-objfileposindep.h"
#include <assert.h>
int
main (void)
{
  int32_t displacement;
  assert (mulib_objfileposindep_rel32 (100, 0, 104, &displacement)
          && displacement == -4);
  return 0;
}
