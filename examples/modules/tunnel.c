#define MULIB_IMPL_XTKS
#define MULIB_IMPL_TUNNEL
#include "mulib-tunnel.h"
#include <assert.h>
static int
read_eof (void *ctx, void *p, size_t n, size_t *count)
{
  (void)ctx;
  (void)p;
  (void)n;
  *count = 0;
  return MULIB_XTKS_EOF;
}
static int
write_bytes (void *ctx, const void *p, size_t n, size_t *count)
{
  (void)ctx;
  (void)p;
  *count = n;
  return 0;
}
int
main (void)
{
  mulib_xtks_t peer = { NULL, read_eof, write_bytes };
  mulib_tunnel_t tunnel;
  char ab[32], ba[32];
  assert (!mulib_tunnel_init (&tunnel, &peer, &peer, ab, ba, sizeof ab));
  assert (mulib_tunnel_step (&tunnel) == MULIB_XTKS_EOF);
  return 0;
}
