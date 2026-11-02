/** @file mulib-upstream.h
 * @brief Borrowed upstream endpoints and bounded in-flight accounting.
 * No sockets or DNS. Define MULIB_IMPL_UPSTREAM. */
#ifndef MULIB_UPSTREAM_H
#define MULIB_UPSTREAM_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_upstream
  {
    const char *host;
    unsigned port;
    size_t active, max_active; /* zero maximum is unlimited */
    int healthy;
  } mulib_upstream_t;
  int mulib_upstream_available (const mulib_upstream_t *upstream);
  int mulib_upstream_acquire (mulib_upstream_t *upstream);
  int mulib_upstream_release (mulib_upstream_t *upstream);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UPSTREAM) && !defined(MULIB_UPSTREAM_IMPLEMENTED)
#define MULIB_UPSTREAM_IMPLEMENTED
#include <stdint.h>
int
mulib_upstream_available (const mulib_upstream_t *u)
{
  return u && u->host && *u->host && u->port && u->port <= 65535 && u->healthy
         && u->active < SIZE_MAX
         && (!u->max_active || u->active < u->max_active);
}
int
mulib_upstream_acquire (mulib_upstream_t *u)
{
  if (!mulib_upstream_available (u))
    return 0;
  ++u->active;
  return 1;
}
int
mulib_upstream_release (mulib_upstream_t *u)
{
  if (!u || !u->active)
    return 0;
  --u->active;
  return 1;
}
#endif
