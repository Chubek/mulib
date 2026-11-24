/** @file mulib-net-cap.h
 * @brief Inspect effective Linux capability bits without changing credentials
 * C11, single-header, define MULIB_IMPL_NET_CAP exactly once.
 */
#ifndef MULIB_NET_CAP_H
#define MULIB_NET_CAP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_net_cap_effective (uint64_t *bits);
  int mulib_net_cap_has (unsigned capability, int *yes);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NET_CAP) && !defined(MULIB_NET_CAP_IMPLEMENTED)
#define MULIB_NET_CAP_IMPLEMENTED
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int
mulib_net_cap_effective (uint64_t *out)
{
  if (!out)
    return -EINVAL;
#ifdef __linux__
  FILE *f = fopen ("/proc/self/status", "r");
  if (!f)
    return -errno;
  char line[256];
  while (fgets (line, sizeof line, f))
    {
      if (strncmp (line, "CapEff:", 7) == 0)
        {
          char *end;
          unsigned long long v = strtoull (line + 7, &end, 16);
          fclose (f);
          if (end == line + 7)
            return -EBADMSG;
          *out = (uint64_t)v;
          return 0;
        }
    }
  fclose (f);
  return -ENOENT;
#else
  return -ENOSYS;
#endif
}
int
mulib_net_cap_has (unsigned cap, int *yes)
{
  if (!yes || cap >= 64)
    return -EINVAL;
  uint64_t bits = 0;
  int r = mulib_net_cap_effective (&bits);
  if (!r)
    *yes = (int)((bits >> cap) & 1u);
  return r;
}
#endif
