#include "include-mulib-zstd.h"
#include <assert.h>
int main (void)
{
  unsigned char zeros[1024] = { 0 };
  mulib_ct_blob packed = { 0 }, restored = { 0 };
  assert (!mulib_zstd_raw_encode (zeros, sizeof (zeros), &packed, 2048));
  assert (packed.length < 32);
  assert (!mulib_zstd_raw_decode (packed.data, packed.length, &restored, 1024));
  assert (restored.length == 1024 && !memcmp (restored.data, zeros, 1024));
  mulib_ct_blob_free (&packed); mulib_ct_blob_free (&restored);
  return 0;
}
