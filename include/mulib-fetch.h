/** @file mulib-fetch.h
 * @brief Transport-agnostic bounded HTTP retrieval with explicit policy
 * enforcement. C11 single-header module. Define MULIB_IMPL_FETCH in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_FETCH_H
#define MULIB_FETCH_H
#include "mulib-fetch-policy.h"
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef int (*mulib_fetch_transport_fn) (void *ctx, const char *url,
                                           mulib_shylock_buffer_t *body,
                                           unsigned *status,
                                           char *content_type, size_t cap);
  typedef struct mulib_fetch_client
  {
    mulib_fetch_policy_t policy;
    mulib_fetch_transport_fn transport;
    void *ctx;
  } mulib_fetch_client_t;
  /** @brief See manual/mulib-fetch.md for this operation's contract. */
  int mulib_fetch_get (const mulib_fetch_client_t *client, const char *url,
                       mulib_shylock_buffer_t *body, unsigned *status,
                       char *content_type, size_t cap);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FETCH_H */
#ifdef MULIB_IMPL_FETCH
#ifndef MULIB_IMPL_FETCH_ONCE
#define MULIB_IMPL_FETCH_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_fetch_get (const mulib_fetch_client_t *c, const char *url,
                 mulib_shylock_buffer_t *body, unsigned *status, char *type,
                 size_t cap)
{
  if (!c || !body || !status || !type || !cap || !c->transport
      || !mulib_fetch_policy_check (&c->policy, url))
    return 0;
  mulib_shylock_buffer_t tmp = { 0 };
  size_t limit = c->policy.max_bytes ? c->policy.max_bytes : 16 * 1024 * 1024;
  tmp.limit = limit;
  char ct[128] = { 0 };
  unsigned st = 0;
  int ok = c->transport (c->ctx, url, &tmp, &st, ct, sizeof ct);
  if (ok && st >= 100 && st <= 599 && memchr (ct, 0, sizeof ct)
      && strlen (ct) < cap && !strpbrk (ct, "\r\n") && tmp.len <= limit
      && tmp.len < tmp.cap && tmp.data)
    {
      if (body->len <= SIZE_MAX - tmp.len)
        ok = mulib_shylock_append (body, tmp.data, tmp.len);
      else
        ok = 0;
      if (ok)
        {
          *status = st;
          memmove (type, ct, strlen (ct) + 1);
        }
    }
  else if (ok && !tmp.len && st >= 100 && st <= 599
           && memchr (ct, 0, sizeof ct) && strlen (ct) < cap
           && !strpbrk (ct, "\r\n"))
    {
      *status = st;
      memmove (type, ct, strlen (ct) + 1);
    }
  else
    ok = 0;
  mulib_shylock_buffer_free (&tmp);
  return ok;
}
#endif
#endif
