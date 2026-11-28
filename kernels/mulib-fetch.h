/** @file mulib-fetch.h
 * @brief Transport-agnostic bounded HTTP retrieval with explicit policy
 * enforcement. C11 single-header module. Define MULIB_IMPL_FETCH in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_FETCH_H
#define MULIB_FETCH_H
#include "mulib-fetch-policy.h"
#include "mulib-bounded-text.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef int (*mulib_fetch_transport_fn) (void *ctx, const char *url,
                                           mulib_bounded_text_buffer_t *body,
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
                       mulib_bounded_text_buffer_t *body, unsigned *status,
                       char *content_type, size_t cap);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FETCH_H */
#ifdef MULIB_IMPL_FETCH
#ifndef MULIB_IMPL_FETCH_ONCE
#define MULIB_IMPL_FETCH_ONCE
#include <string.h>

static int
mulib_fetch_content_type_ (const char *s, size_t len)
{
  for (size_t i = 0; i < len; i++)
    {
      unsigned char c = (unsigned char)s[i];
      if ((c < 0x20 && c != '\t') || c == 0x7f)
        return 0;
    }
  return 1;
}

int
mulib_fetch_get (const mulib_fetch_client_t *c, const char *url,
                 mulib_bounded_text_buffer_t *body, unsigned *status, char *type,
                 size_t cap)
{
  if (!c || !body || !status || !type || !cap || !c->transport
      || !mulib_fetch_policy_check (&c->policy, url))
    return 0;
  mulib_bounded_text_buffer_t tmp = { 0 };
  size_t limit = c->policy.max_bytes ? c->policy.max_bytes : 16 * 1024 * 1024;
  tmp.limit = limit;
  char ct[128] = { 0 };
  const char *end;
  unsigned st = 0;
  int ok = 0;
  if (c->transport (c->ctx, url, &tmp, &st, ct, sizeof ct) != 1
      || st < 200 || st > 599 || tmp.len > limit
      || (tmp.len && (st == 204 || st == 205 || st == 304)))
    goto done;
  if (tmp.data ? tmp.len >= tmp.cap : (tmp.len || tmp.cap))
    goto done;
  end = (const char *)memchr (ct, 0, sizeof ct);
  if (end)
    {
      size_t len = (size_t)(end - ct);
      if (len < cap && mulib_fetch_content_type_ (ct, len)
          && (!tmp.len || mulib_bounded_text_append (body, tmp.data, tmp.len)))
        {
          *status = st;
          memmove (type, ct, len + 1);
          ok = 1;
        }
    }
done:
  mulib_bounded_text_buffer_free (&tmp);
  return ok;
}
#endif
#endif
