/** @file mulib-websearch-provider.h
 * @brief Registered search providers with encoded query-URL generation.
 * C11 single-header module. Define MULIB_IMPL_WEBSEARCH_PROVIDER in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_WEBSEARCH_PROVIDER_H
#define MULIB_WEBSEARCH_PROVIDER_H
#include "mulib-browser-origin.h"
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_websearch_provider_config
  {
    char name[32], endpoint[512];
    unsigned max_results;
  } mulib_websearch_provider_config_t;
  /** @brief See manual/mulib-websearch-provider.md for this operation's
   * contract. */
  int mulib_websearch_provider_init (mulib_websearch_provider_config_t *out,
                                     const char *name, const char *endpoint,
                                     unsigned limit);
  /** @brief See manual/mulib-websearch-provider.md for this operation's
   * contract. */
  int mulib_websearch_provider_url (const mulib_websearch_provider_config_t *provider,
                                    const char *query, unsigned offset,
                                    mulib_shylock_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_WEBSEARCH_PROVIDER_H */
#ifdef MULIB_IMPL_WEBSEARCH_PROVIDER
#ifndef MULIB_IMPL_WEBSEARCH_PROVIDER_ONCE
#define MULIB_IMPL_WEBSEARCH_PROVIDER_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_websearch_provider_init (mulib_websearch_provider_config_t *o, const char *n,
                               const char *url, unsigned limit)
{
  if (!o || !n || !*n || !url || !limit || strchr (url, '#'))
    return 0;
  mulib_browser_origin_t origin;
  if (!mulib_browser_origin_parse (url, &origin)
      || strcmp (origin.scheme, "https"))
    return 0;
  mulib_websearch_provider_config_t z = { 0 };
  if (!mulib_shylock_copy (z.name, sizeof z.name, n)
      || !mulib_shylock_copy (z.endpoint, sizeof z.endpoint, url))
    return 0;
  z.max_results = limit;
  *o = z;
  return 1;
}
int
mulib_websearch_provider_url (const mulib_websearch_provider_config_t *p,
                              const char *q, unsigned offset,
                              mulib_shylock_buffer_t *b)
{
  if (!p || !memchr (p->endpoint, 0, sizeof p->endpoint)
      || !memchr (p->name, 0, sizeof p->name) || !p->max_results || !q || !b
      || strlen (q) > 4096 || offset > 100000)
    return 0;
  mulib_websearch_provider_config_t checked;
  if (!mulib_websearch_provider_init (&checked, p->name, p->endpoint,
                                      p->max_results))
    return 0;
  mulib_shylock_buffer_t v = { 0 };
  v.limit = b->limit;
  int ok = mulib_shylock_text (&v, p->endpoint);
  if (ok)
    ok = mulib_shylock_text (&v, strchr (p->endpoint, '?') ? "&q=" : "?q=");
  const char *hex = "0123456789ABCDEF";
  for (const unsigned char *s = (const unsigned char *)q; ok && *s; s++)
    {
      if ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z')
          || (*s >= '0' && *s <= '9') || *s == '-' || *s == '_' || *s == '.'
          || *s == '~')
        ok = mulib_shylock_append (&v, s, 1);
      else if (*s == ' ')
        ok = mulib_shylock_text (&v, "+");
      else
        {
          char z[3] = { '%', hex[*s >> 4], hex[*s & 15] };
          ok = mulib_shylock_append (&v, z, 3);
        }
    }
  char tail[64];
  snprintf (tail, sizeof tail, "&start=%u", offset);
  if (ok)
    ok = mulib_shylock_text (&v, tail);
  if (ok)
    ok = mulib_shylock_append (b, v.data, v.len);
  mulib_shylock_buffer_free (&v);
  return ok;
}
#endif
#endif
