#define MULIB_IMPL_MACHOBUIDLER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_BUFFER
#include "mulib-machobuidler.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t object = { 0 };
  const unsigned char ret[] = { 0xc3 };
  assert (mulib_machobuilder_text (&object, ret, 1));
  assert (object.length == 193 && object.data[192] == 0xc3);
  mulib_buffer_free (&object);
  return 0;
}
