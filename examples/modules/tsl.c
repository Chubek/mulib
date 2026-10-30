#define MULIB_IMPL_TSL
#include "mulib-tsl.h"
#include <assert.h>
/* Deliberately unverified test backend: proves failure-closed policy, NOT TLS.
 */
static int
handshake (void *ctx, int *verified)
{
  (void)ctx;
  *verified = 0;
  return 0;
}
static int
read_data (void *ctx, void *p, size_t n, size_t *got)
{
  (void)ctx;
  (void)p;
  (void)n;
  *got = 0;
  return -1;
}
static int
write_data (void *ctx, const void *p, size_t n, size_t *got)
{
  (void)ctx;
  (void)p;
  (void)n;
  *got = 0;
  return -1;
}
static void
close_session (void *ctx)
{
  (void)ctx;
}
int
main (void)
{
  mulib_tsl_t s = { 0 };
  mulib_tsl_ops_t ops = { handshake, read_data, write_data, close_session };
  assert (mulib_tsl_init (&s, &ops, NULL));
  assert (mulib_tsl_handshake (&s) == MULIB_TSL_EAUTH);
  mulib_tsl_close (&s);
  return 0;
}
