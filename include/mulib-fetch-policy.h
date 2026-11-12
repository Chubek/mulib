/** @file mulib-fetch-policy.h
 * @brief Conservative outbound URL security checks and restricted fetch
 * policy. C11 single-header module. Define MULIB_IMPL_FETCH_POLICY in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_FETCH_POLICY_H
#define MULIB_FETCH_POLICY_H
#include "mulib-browser-origin.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_fetch_policy
  {
    size_t max_url, max_bytes;
    int allow_http, allow_https, allow_private;
  } mulib_fetch_policy_t;
  /** @brief See manual/mulib-fetch-policy.md for this operation's contract. */
  int mulib_fetch_policy_check (const mulib_fetch_policy_t *p,
                                const char *url);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FETCH_POLICY_H */
#ifdef MULIB_IMPL_FETCH_POLICY
#ifndef MULIB_IMPL_FETCH_POLICY_ONCE
#define MULIB_IMPL_FETCH_POLICY_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
mulib_fetch_policy_local_ (const char *host)
{
  if (!host || !host[0])
    return 1;
  size_t n = strlen (host);
  if (!strcmp (host, "localhost") || !strcmp (host, "localhost.")
      || !strcmp (host, "localhost.localdomain")
      || (n >= 10 && !strcmp (host + n - 10, ".localhost"))
      || (n >= 6 && !strcmp (host + n - 6, ".local")))
    return 1;
  if (strchr (host, ':'))
    return 1; /* No IPv6 classification yet: fail closed. */
  unsigned a[4] = { 0 };
  size_t j = 0;
  const char *s = host;
  while (j < 4)
    {
      const char *start = s;
      unsigned x = 0;
      size_t count = 0;
      while (*s >= '0' && *s <= '9')
        {
          x = x * 10 + (unsigned)(*s++ - '0');
          count++;
          if (x > 255 || count > 3)
            return 1;
        }
      if (!count || (count > 1 && *start == '0'))
        break;
      a[j++] = x;
      if (j < 4)
        {
          if (*s != '.')
            break;
          s++;
        }
    }
  if (j == 4 && !(*s))
    return a[0] == 0 || a[0] == 10 || a[0] == 127 || a[0] >= 224
           || (a[0] == 169 && a[1] == 254)
           || (a[0] == 172 && a[1] >= 16 && a[1] <= 31)
           || (a[0] == 192 && a[1] == 168)
           || (a[0] == 100 && a[1] >= 64 && a[1] <= 127)
           || (a[0] == 198 && (a[1] == 18 || a[1] == 19));
  /* Reject ambiguous numeric IP spellings. */
  if (isdigit ((unsigned char)host[0]))
    return 1;
  return 0;
}
int
mulib_fetch_policy_check (const mulib_fetch_policy_t *p, const char *url)
{
  if (!p || !url || strlen (url) > (p->max_url ? p->max_url : 8192))
    return 0;
  mulib_browser_origin_t origin;
  if (!mulib_browser_origin_parse (url, &origin))
    return 0;
  if (!strcmp (origin.scheme, "https") ? !p->allow_https : !p->allow_http)
    return 0;
  if (!p->allow_private && mulib_fetch_policy_local_ (origin.host))
    return 0;
  return 1;
}

#endif
#endif
