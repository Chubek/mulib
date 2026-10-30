#define MULIB_IMPL_OBJFILESTUB
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#include "mulib-objfilestub.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t b = { 0 };
  assert (mulib_objfilestub_x64_jump (&b, 0x1234) && b.length == 14);
  mulib_buffer_free (&b);
  return 0;
}
