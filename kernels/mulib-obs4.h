/** @file mulib-obs4.h
 * @brief Authenticated obfs4 backend lifecycle and X transport adaptation.
 * Define MULIB_IMPL_OBS4 and MULIB_IMPL_XTKS once. Historical obs4 filename.
 * The backend supplies obfs4 cryptography, handshake, framing and padding.
 */
#ifndef MULIB_OBS4_H
#define MULIB_OBS4_H
#include "mulib-xtks.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief I/O follows the X transport callback contract. Recursive
   * operations are rejected; close during a callback is deferred until it
   * returns. Callbacks must not mutate/reinitialize the adapter or free its
   * storage. Backend close is invoked exactly once. */
  typedef struct mulib_obs4_ops
  {
    int (*handshake) (void *, const unsigned char certificate[52],
                      int *authenticated);
    int (*read) (void *, void *, size_t, size_t *);
    int (*write) (void *, const void *, size_t, size_t *);
    void (*close) (void *);
  } mulib_obs4_ops_t;
  typedef struct mulib_obs4
  {
    mulib_obs4_ops_t ops;
    void *context;
    unsigned char certificate[52];
    int state;
  } mulib_obs4_t;
  /** @brief Initialize zero-state client with decoded node-id/public-key cert.
   * All callbacks mandatory; copied cert, borrowed backend. 1 success/0
   * invalid. Backend handshake returns OK only after checking the pinned
   * certificate. */
  int mulib_obs4_init (mulib_obs4_t *, const mulib_obs4_ops_t *, void *,
                       const unsigned char certificate[52]);
  /** @brief 0 ready, 1 AGAIN, negative sticky failure. No I/O before ready. */
  int mulib_obs4_handshake (mulib_obs4_t *);
  /** @brief Borrow a checked stream; invalidated when adapter closes. */
  mulib_xtks_t mulib_obs4_stream (mulib_obs4_t *);
  /** @brief Close backend exactly once and clear state; NULL-safe. */
  void mulib_obs4_close (mulib_obs4_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OBS4) && !defined(MULIB_OBS4_IMPLEMENTED)
#define MULIB_OBS4_IMPLEMENTED
#include <string.h>
/* States 4/5/6 mean in callback/deferred close/closing respectively. */
static int
mulib_obs4_return_ (mulib_obs4_t *s, int state)
{
  int closing = s->state == 5;
  s->state = state;
  if (closing)
    mulib_obs4_close (s);
  return !closing;
}
int
mulib_obs4_init (mulib_obs4_t *s, const mulib_obs4_ops_t *ops, void *ctx,
                 const unsigned char cert[52])
{
  if (!s || s->state || !ops || !ops->handshake || !ops->read || !ops->write
      || !ops->close || !cert)
    return 0;
  s->ops = *ops;
  s->context = ctx;
  memcpy (s->certificate, cert, 52);
  s->state = 1;
  return 1;
}
int
mulib_obs4_handshake (mulib_obs4_t *s)
{
  int r, verified = 0;
  if (!s || !s->state || s->state >= 4)
    return -1;
  if (s->state == 2)
    return 0;
  if (s->state != 1)
    return -2;
  s->state = 4;
  r = s->ops.handshake (s->context, s->certificate, &verified);
  if (!mulib_obs4_return_ (s, 1))
    return MULIB_XTKS_EINVAL;
  if (r == 1)
    return 1;
  if (r || verified != 1)
    {
      s->state = 3;
      return -2;
    }
  s->state = 2;
  return 0;
}
static int
mulib_obs4_read_ (void *ctx, void *p, size_t n, size_t *count)
{
  mulib_obs4_t *s = ctx;
  mulib_xtks_t stream;
  int r;
  if (count)
    *count = 0;
  if (!s || s->state != 2 || !count || (!p && n))
    return -1;
  stream.context = s->context;
  stream.read = s->ops.read;
  stream.write = s->ops.write;
  s->state = 4;
  r = mulib_xtks_read (&stream, p, n, count);
  if (!mulib_obs4_return_ (s, 2))
    {
      *count = 0;
      return MULIB_XTKS_EINVAL;
    }
  if (r < 0)
    s->state = 3;
  return r;
}
static int
mulib_obs4_write_ (void *ctx, const void *p, size_t n, size_t *count)
{
  mulib_obs4_t *s = ctx;
  mulib_xtks_t stream;
  int r;
  if (count)
    *count = 0;
  if (!s || s->state != 2 || !count || (!p && n))
    return -1;
  stream.context = s->context;
  stream.read = s->ops.read;
  stream.write = s->ops.write;
  s->state = 4;
  r = mulib_xtks_write (&stream, p, n, count);
  if (!mulib_obs4_return_ (s, 2))
    {
      *count = 0;
      return MULIB_XTKS_EINVAL;
    }
  if (r < 0)
    s->state = 3;
  return r;
}
mulib_xtks_t
mulib_obs4_stream (mulib_obs4_t *s)
{
  mulib_xtks_t t;
  t.context = s;
  t.read = mulib_obs4_read_;
  t.write = mulib_obs4_write_;
  return t;
}
void
mulib_obs4_close (mulib_obs4_t *s)
{
  if (s && s->state == 4)
    s->state = 5;
  else if (s && s->state > 0 && s->state < 4)
    {
      s->state = 6;
      s->ops.close (s->context);
      memset (s, 0, sizeof (*s));
    }
}
#endif
