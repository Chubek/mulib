/** @file mulib-robots.h
 * @brief Simple robots.txt User-agent and Disallow matcher for a selected bot.
 * C11 single-header implementation. Define MULIB_IMPL_ROBOTS in one
 * translation unit.
 */
#ifndef MULIB_ROBOTS_H
#define MULIB_ROBOTS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Literal path-prefix rules with longest match (Allow wins ties).
   * Exact case-insensitive User-agent groups override wildcard groups;
   * repeated matching groups are combined and consecutive agent lines share
   * rules. Comments/CRLF supported; no path wildcards, '$', or percent
   * decoding. 1 allowed, 0 disallowed/invalid. No fetching; not an
   * access-control boundary. */
  int mulib_robots_allowed (const char *robots, const char *agent,
                            const char *path);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ROBOTS) && !defined(MULIB_ROBOTS_IMPLEMENTED)
#define MULIB_ROBOTS_IMPLEMENTED
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  static int
  mulib_robots_eq_ (const char *a, size_t n, const char *b)
  {
    size_t m = strlen (b);
    if (n != m)
      return 0;
    for (size_t i = 0; i < n; i++)
      {
        unsigned x = (unsigned char)a[i], y = (unsigned char)b[i];
        if (x >= 'A' && x <= 'Z')
          x += 32;
        if (y >= 'A' && y <= 'Z')
          y += 32;
        if (x != y)
          return 0;
      }
    return 1;
  }
  int
  mulib_robots_allowed (const char *r, const char *agent, const char *path)
  {
    if (!r || !agent || !*agent || !path || *path != '/')
      return 0;
    int exact = 0, wild = 0, rules = 0, have_exact = 0, deny[2] = { 0, 0 };
    size_t best[2] = { 0, 0 }, plen = strlen (path);
    while (*r)
      {
        const char *line = strchr (r, '\n');
        if (!line)
          line = r + strlen (r);
        const char *e = (const char *)memchr (r, '#', (size_t)(line - r));
        if (!e)
          e = line;
        const char *a = r;
        while (a < e && isspace ((unsigned char)*a))
          ++a;
        const char *c = (const char *)memchr (a, ':', (size_t)(e - a));
        if (c)
          {
            const char *keyend = c, *v = c + 1, *z = e;
            while (keyend > a && isspace ((unsigned char)keyend[-1]))
              --keyend;
            while (v < z && isspace ((unsigned char)*v))
              ++v;
            while (z > v && isspace ((unsigned char)z[-1]))
              --z;
            if (mulib_robots_eq_ (a, (size_t)(keyend - a), "User-agent"))
              {
                if (rules)
                  {
                    exact = wild = rules = 0;
                  }
                if (mulib_robots_eq_ (v, (size_t)(z - v), agent))
                  {
                    exact = 1;
                    have_exact = 1;
                  }
                if (mulib_robots_eq_ (v, (size_t)(z - v), "*"))
                  wild = 1;
              }
            else
              {
                rules = 1;
                int allow
                    = mulib_robots_eq_ (a, (size_t)(keyend - a), "Allow");
                if (allow
                    || mulib_robots_eq_ (a, (size_t)(keyend - a), "Disallow"))
                  {
                    size_t n = (size_t)(z - v);
                    if (n && n <= plen && !memcmp (path, v, n))
                      for (int i = 0; i < 2; ++i)
                        if ((i ? exact : wild)
                            && (n > best[i] || (n == best[i] && allow)))
                          {
                            best[i] = n;
                            deny[i] = !allow;
                          }
                  }
              }
          }
        r = *line ? line + 1 : line;
      }
    return !deny[have_exact ? 1 : 0];
  }

#ifdef __cplusplus
}
#endif
#endif
