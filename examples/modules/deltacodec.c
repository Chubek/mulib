#define MULIB_IMPL_DELTACODEC
#include "mulib-deltacodec.h"
#include <assert.h>
int
main (void)
{
  uint64_t ids[] = { 10, 10, 17 }, delta[3], decoded[3];
  assert (mulib_deltacodec_encode (ids, 3, delta) && delta[1] == 0 && delta[2] == 7);
  assert (mulib_deltacodec_decode (delta, 3, decoded)
          && !memcmp (ids, decoded, sizeof (ids)));
  return 0;
}
