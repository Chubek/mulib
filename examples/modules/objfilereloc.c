#define MULIB_IMPL_OBJFILERELOC
#define MULIB_IMPL_OBJFILEPOSINDEP
#define MULIB_IMPL_BYCUTILS
#include "mulib-bycutils.h"
#include "mulib-objfilereloc.h"
#include <assert.h>
int
main (void)
{
  unsigned char b[4];
  uint64_t value;
  assert (
      mulib_objfilereloc_apply (b, 4, 0, MULIB_OBJFILERELOC_ABS32, 40, 2, 0));
  assert (mulib_bycutils_load (b, 4, 0, 4, &value) && value == 42);
  return 0;
}
