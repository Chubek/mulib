#define MULIB_IMPL_PEBUILDER
#define MULIB_IMPL_BINFILEWRITER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_BUFFER
#include "mulib-pebuilder.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t image = { 0 };
  const unsigned char code[] = { 0x31, 0xc0, 0xc3 };
  assert (mulib_pebuilder_text (&image, code, sizeof (code), 0));
  assert (image.length == 1024 && !memcmp (image.data, "MZ", 2));
  mulib_buffer_free (&image);
  return 0;
}
