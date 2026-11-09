#define MULIB_IMPL_CODEC_CORE
#define MULIB_IMPL_BUFFER
#include "mulib-codec-core.h"
#include <assert.h>
int main (void)
{
  mulib_buffer_t bytes={0};
  assert (!mulib_codec_append (&bytes,"abc",3,9));
  assert (!mulib_codec_match (&bytes,3,6,9));
  assert (!memcmp (bytes.data,"abcabcabc",9));
  assert (mulib_codec_crc64 ("123456789",9)==UINT64_C(0x995dc9bbdf1939fa));
  mulib_buffer_free (&bytes); return 0;
}
