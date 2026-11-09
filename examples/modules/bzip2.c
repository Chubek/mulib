#define MULIB_IMPL_BZIP2
#define MULIB_IMPL_CODEC_CORE
#define MULIB_IMPL_BUFFER
#include "mulib-bzip2.h"
#include <assert.h>
int main (void)
{
  mulib_buffer_t packed={0},plain={0};
  assert (!mulib_bzip2_encode ("hello hello hello",17,&packed));
  assert (!mulib_bzip2_decode (packed.data,packed.length,17,&plain));
  assert (plain.length==17 && !memcmp (plain.data,"hello hello hello",17));
  mulib_buffer_free (&packed); mulib_buffer_free (&plain); return 0;
}
