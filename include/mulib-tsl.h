/** @file mulib-tsl.h
 * @brief Transport-security lifecycle adapter for an application-supplied TLS
 * backend. Define MULIB_IMPL_TSL once. Historical spelling TSL; no
 * cryptographic implementation or insecure plaintext fallback is supplied.
 * Backend must implement authenticated TLS. */
#ifndef MULIB_TSL_H
#define MULIB_TSL_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_TSL_OK = 0,
    MULIB_TSL_AGAIN = 1,
    MULIB_TSL_EOF = 2,
    MULIB_TSL_EINVAL = -1,
    MULIB_TSL_EAUTH = -2,
    MULIB_TSL_EBACKEND = -3
  };
  /** @brief Borrowed callbacks/context, must outlive adapter. Handshake
   * returns OK only after hostname/chain verification, with verified=1. AGAIN
   * is resumable. I/O returns OK/AGAIN/EOF or negative; count may never exceed
   * capacity. Nonempty OK I/O must make progress; other statuses require zero
   * count. Zero-length I/O does not invoke the backend. Recursive operations
   * are rejected; close during a callback is deferred until it returns.
   * Callbacks must not mutate/reinitialize the adapter or free its storage.
   * close releases the backend session exactly once. */
  typedef struct mulib_tsl_ops
  {
    int (*handshake) (void *, int *verified);
    int (*read) (void *, void *, size_t, size_t *);
    int (*write) (void *, const void *, size_t, size_t *);
    void (*close) (void *);
  } mulib_tsl_ops_t;
  typedef struct mulib_tsl
  {
    mulib_tsl_ops_t ops;
    void *context;
    int state;
  } mulib_tsl_t;
  /** @brief Initialize inactive adapter; all callbacks mandatory; 1 success.
   */
  int mulib_tsl_init (mulib_tsl_t *s, const mulib_tsl_ops_t *ops,
                      void *context);
  /** @brief Establish verified session before permitting I/O; failure becomes
   * sticky. */
  int mulib_tsl_handshake (mulib_tsl_t *s);
  /** @brief Read authenticated session only; backend cannot be bypassed. */
  int mulib_tsl_read (mulib_tsl_t *s, void *data, size_t capacity,
                      size_t *count);
  /** @brief Write authenticated session only; short writes/AGAIN allowed. */
  int mulib_tsl_write (mulib_tsl_t *s, const void *data, size_t size,
                       size_t *count);
  /** @brief Close/reset once; zero-initialized/closed objects accepted. */
  void mulib_tsl_close (mulib_tsl_t *s);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TSL) && !defined(MULIB_TSL_IMPLEMENTED)
#define MULIB_TSL_IMPLEMENTED
#include <string.h>
/* States: 0 inactive, 1 handshake, 2 ready, 3 failed, 4 in callback,
 * 5 deferred close, 6 closing. Keep backend lifetime through callback return. */
static int
mulib_tsl_return_ (mulib_tsl_t *s, int state)
{
  int closing = s->state == 5;
  s->state = state;
  if (closing)
    mulib_tsl_close (s);
  return !closing;
}
int
mulib_tsl_init (mulib_tsl_t *s, const mulib_tsl_ops_t *ops, void *ctx)
{
  if (!s || s->state || !ops || !ops->handshake || !ops->read || !ops->write
      || !ops->close)
    return 0;
  s->ops = *ops;
  s->context = ctx;
  s->state = 1;
  return 1;
}
int
mulib_tsl_handshake (mulib_tsl_t *s)
{
  int rc, verified = 0;
  if (!s || !s->state || s->state >= 4)
    return -1;
  if (s->state == 2)
    return 0;
  if (s->state != 1)
    return -2;
  s->state = 4;
  rc = s->ops.handshake (s->context, &verified);
  if (!mulib_tsl_return_ (s, 1))
    return MULIB_TSL_EINVAL;
  if (rc == MULIB_TSL_AGAIN)
    return rc;
  if (rc != 0 || verified != 1)
    {
      s->state = 3;
      return -2;
    }
  s->state = 2;
  return 0;
}
int
mulib_tsl_read (mulib_tsl_t *s, void *p, size_t n, size_t *count)
{
  int rc;
  size_t got = 0;
  if (count)
    *count = 0;
  if (!s || s->state != 2 || !count || (!p && n))
    return -1;
  if (!n)
    return MULIB_TSL_OK;
  s->state = 4;
  rc = s->ops.read (s->context, p, n, &got);
  if (!mulib_tsl_return_ (s, 2))
    return MULIB_TSL_EINVAL;
  if (got > n || rc > 2 || (rc != 0 && got) || (!rc && !got))
    {
      s->state = 3;
      return -3;
    }
  if (rc < 0)
    {
      s->state = 3;
      return rc;
    }
  if (!rc)
    *count = got;
  return rc;
}
int
mulib_tsl_write (mulib_tsl_t *s, const void *p, size_t n, size_t *count)
{
  int rc;
  size_t got = 0;
  if (count)
    *count = 0;
  if (!s || s->state != 2 || !count || (!p && n))
    return -1;
  if (!n)
    return MULIB_TSL_OK;
  s->state = 4;
  rc = s->ops.write (s->context, p, n, &got);
  if (!mulib_tsl_return_ (s, 2))
    return MULIB_TSL_EINVAL;
  if (got > n || rc > 1 || (rc != 0 && got) || (!rc && !got))
    {
      s->state = 3;
      return -3;
    }
  if (rc < 0)
    {
      s->state = 3;
      return rc;
    }
  if (!rc)
    *count = got;
  return rc;
}
void
mulib_tsl_close (mulib_tsl_t *s)
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
