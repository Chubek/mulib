#define MULIB_IMPL_VARINT
#define MULIB_IMPL_P2PPROTO
#include "mulib-p2pproto.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  const char *supported[] = { "/ipfs/bitswap/1.2.0" };
  char token[128];
  unsigned char wire[128];
  size_t n, used;
  assert (mulib_p2pproto_encode (supported[0], wire, sizeof wire, &n));
  assert (mulib_p2pproto_decode (wire, n, token, sizeof token, &used) == 1
          && used == n);
  assert (mulib_p2pproto_select (token, supported, 1) == 0);
  return 0;
}
