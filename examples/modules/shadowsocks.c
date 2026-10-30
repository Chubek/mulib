#define MULIB_IMPL_SHADOWSOCKS
#include "mulib-shadowsocks.h"
#include <assert.h>
/* A rejecting test backend demonstrates fail-closed behavior, not encryption.
 */
static int
reject (void *ctx, const unsigned char nonce[12], const void *p, size_t n,
        void *out)
{
  (void)ctx;
  (void)nonce;
  (void)p;
  (void)n;
  (void)out;
  return 0;
}
int
main (void)
{
  mulib_shadowsocks_t stream;
  unsigned char out[64];
  size_t used;
  assert (mulib_shadowsocks_init (&stream, 0, reject, NULL));
  assert (mulib_shadowsocks_seal (&stream, "hello", 5, out, sizeof out, &used)
          == -2);
  assert (stream.failed && used == 0 && stream.nonce == 0);
  return 0;
}
