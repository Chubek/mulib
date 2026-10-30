#define MULIB_IMPL_XTKS
#define MULIB_IMPL_OBS4
#include "mulib-obs4.h"
#include <assert.h>
/* Lifecycle fixture, not an obfs4 cryptographic implementation. */
static int
handshake (void *ctx, const unsigned char cert[52], int *verified)
{
  (void)ctx;
  (void)cert;
  *verified = 0;
  return -1;
}
static int
read_bytes (void *ctx, void *p, size_t n, size_t *got)
{
  (void)ctx;
  (void)p;
  (void)n;
  *got = 0;
  return 1;
}
static int
write_bytes (void *ctx, const void *p, size_t n, size_t *got)
{
  (void)ctx;
  (void)p;
  (void)n;
  *got = 0;
  return 1;
}
static void
close_backend (void *ctx)
{
  ++*(int *)ctx;
}
int
main (void)
{
  mulib_obs4_t client = { 0 };
  unsigned char cert[52] = { 0 };
  int closed = 0;
  mulib_obs4_ops_t ops = { handshake, read_bytes, write_bytes, close_backend };
  assert (mulib_obs4_init (&client, &ops, &closed, cert));
  assert (mulib_obs4_handshake (&client) < 0);
  mulib_obs4_close (&client);
  mulib_obs4_close (&client);
  assert (closed == 1);
  return 0;
}
