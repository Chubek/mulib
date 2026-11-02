#define MULIB_IMPL_COFFBUILDER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_BUFFER
#include "mulib-coffbuilder.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t object = { 0 };
  const unsigned char ret[] = { 0xc3 };
  assert (mulib_coffbuilder_text (&object, ret, 1));
  assert (object.length == 61 && object.data[60] == 0xc3);
  mulib_buffer_free (&object);
  return 0;
}
