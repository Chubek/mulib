/** @file mulib-browser-cookiejar.h
 * @brief Host/domain/path/secure-aware in-memory browser cookie storage.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_COOKIEJAR in exactly one
 * C translation unit. A bounded host/domain/path profile, not full RFC6265bis
 * or SameSite semantics.
 */
#ifndef MULIB_BROWSER_COOKIEJAR_H
#define MULIB_BROWSER_COOKIEJAR_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_cookie
  {
    char name[128], value[512], domain[256], path[256];
    int64_t expiry_unix;
    unsigned secure, host_only, http_only;
  } mulib_browser_cookie_t;
  typedef struct mulib_browser_cookiejar
  {
    mulib_browser_cookie_t *items;
    size_t len, cap, limit;
  } mulib_browser_cookiejar_t;
  /** @brief See manual/mulib-browser-cookiejar.md for this operation's
   * contract. */
  int mulib_browser_cookiejar_put (mulib_browser_cookiejar_t *j,
                                   const mulib_browser_cookie_t *cookie);
  /** @brief See manual/mulib-browser-cookiejar.md for this operation's
   * contract. */
  int mulib_browser_cookiejar_header (const mulib_browser_cookiejar_t *j,
                                      const char *hostname, const char *path,
                                      int tls, int64_t now,
                                      mulib_shylock_buffer_t *out);
  /** @brief See manual/mulib-browser-cookiejar.md for this operation's
   * contract. */
  void mulib_browser_cookiejar_free (mulib_browser_cookiejar_t *j);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_COOKIEJAR_H */
#ifdef MULIB_IMPL_BROWSER_COOKIEJAR
#ifndef MULIB_IMPL_BROWSER_COOKIEJAR_ONCE
#define MULIB_IMPL_BROWSER_COOKIEJAR_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
mulib_browser_cookiejar_domain_ (const char *host, const char *domain,
                                 int only)
{
  if (only)
    return !strcmp (host, domain);
  size_t h = strlen (host), d = strlen (domain);
  return h >= d && !strcmp (host + h - d, domain)
         && (h == d || host[h - d - 1] == '.');
}
static int
mulib_browser_cookiejar_safe_ (const char *s, int value)
{
  for (; *s; s++)
    {
      unsigned char ch = (unsigned char)*s;
      if (ch < 0x21 || ch >= 0x7f
          || strchr (value ? "\";,\\" : "()<>@,;:\\[?]={}\"/", ch))
        return 0;
    }
  return 1;
}
int
mulib_browser_cookiejar_put (mulib_browser_cookiejar_t *j,
                             const mulib_browser_cookie_t *c)
{
  if (!j || !c || !memchr (c->name, 0, sizeof c->name)
      || !memchr (c->value, 0, sizeof c->value)
      || !memchr (c->domain, 0, sizeof c->domain)
      || !memchr (c->path, 0, sizeof c->path) || !c->name[0] || !c->domain[0]
      || c->path[0] != '/' || c->expiry_unix < 0
      || !mulib_browser_cookiejar_safe_ (c->name, 0)
      || !mulib_browser_cookiejar_safe_ (c->value, 1))
    return 0;
  mulib_browser_cookie_t normalized = *c;
  c = &normalized;
  size_t d = strlen (c->domain);
  for (size_t i = 0; i < d; i++)
    {
      unsigned char ch = (unsigned char)c->domain[i];
      if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
            || (ch >= '0' && ch <= '9') || ch == '-' || ch == '.'))
        return 0;
      normalized.domain[i] = (char)tolower (ch);
    }
  if (c->domain[0] == '.' || c->domain[d - 1] == '.'
      || strstr (c->domain, ".."))
    return 0;
  for (size_t i = 0; c->path[i]; i++)
    if ((unsigned char)c->path[i] < 0x21 || (unsigned char)c->path[i] >= 0x7f
        || c->path[i] == ';')
      return 0;
  for (size_t i = 0; i < j->len; i++)
    if (!strcmp (j->items[i].name, c->name)
        && !strcmp (j->items[i].domain, c->domain)
        && !strcmp (j->items[i].path, c->path))
      {
        j->items[i] = *c;
        return 1;
      }
  if (j->limit && j->len >= j->limit)
    return 0;
  if (j->len == j->cap)
    {
      size_t n = j->cap ? j->cap * 2 : 16;
      if (n < j->cap || n > SIZE_MAX / sizeof *j->items)
        return 0;
      if (j->limit && n > j->limit)
        n = j->limit;
      void *p = realloc (j->items, n * sizeof *j->items);
      if (!p)
        return 0;
      j->items = p;
      j->cap = n;
    }
  j->items[j->len++] = *c;
  return 1;
}
int
mulib_browser_cookiejar_header (const mulib_browser_cookiejar_t *j,
                                const char *host, const char *path, int tls,
                                int64_t now, mulib_shylock_buffer_t *out)
{
  if (!j || !host || !*host || strlen (host) > 253 || !path || path[0] != '/'
      || !out || now < 0)
    return 0;
  char hostname[256];
  size_t hn = strlen (host);
  for (size_t i = 0; i <= hn; i++)
    hostname[i] = (char)tolower ((unsigned char)host[i]);
  if (hn && hostname[hn - 1] == '.')
    hostname[hn - 1] = 0;
  mulib_shylock_buffer_t b = { 0 };
  b.limit = out->limit;
  for (size_t i = 0; i < j->len; i++)
    {
      const mulib_browser_cookie_t *c = j->items + i;
      size_t plen = strlen (c->path);
      if ((c->expiry_unix && now >= c->expiry_unix) || (c->secure && !tls)
          || !mulib_browser_cookiejar_domain_ (hostname, c->domain,
                                               c->host_only)
          || strncmp (path, c->path, plen)
          || (c->path[plen - 1] != '/' && path[plen] && path[plen] != '/'))
        continue;
      if (b.len && !mulib_shylock_text (&b, "; "))
        goto fail;
      if (!mulib_shylock_text (&b, c->name) || !mulib_shylock_text (&b, "=")
          || !mulib_shylock_text (&b, c->value))
        goto fail;
    }
  int ok = mulib_shylock_append (out, b.data, b.len);
  mulib_shylock_buffer_free (&b);
  return ok;
fail:
  mulib_shylock_buffer_free (&b);
  return 0;
}
void
mulib_browser_cookiejar_free (mulib_browser_cookiejar_t *j)
{
  if (j)
    {
      free (j->items);
      memset (j, 0, sizeof *j);
    }
}
#endif
#endif
