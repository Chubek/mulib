/** @file mulib-ethtool-stats.h
 * @brief Read interface receive byte statistics from sysfs (not complete
 * driver ethtool counters) C11, single-header, define MULIB_IMPL_ETHTOOL_STATS
 * exactly once.
 */
#ifndef MULIB_ETHTOOL_STATS_H
#define MULIB_ETHTOOL_STATS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_ethtool_stats_sysfs (const char *ifname, char *buf, size_t cap);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ETHTOOL_STATS)                                         \
    && !defined(MULIB_ETHTOOL_STATS_IMPLEMENTED)
#define MULIB_ETHTOOL_STATS_IMPLEMENTED
#include <errno.h>
#include <stdio.h>
#include <string.h>
int
mulib_ethtool_stats_sysfs (const char *name, char *out, size_t cap)
{
  if (!name || !out || cap < 2 || !name[0] || strlen (name) > 15)
    return -EINVAL;
  for (const char *p = name; *p; p++)
    if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
          || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' || *p == '.'))
      return -EINVAL;
#ifdef __linux__
  char path[256];
  snprintf (path, sizeof path, "/sys/class/net/%s/statistics/rx_bytes", name);
  FILE *fp = fopen (path, "r");
  if (!fp)
    return -errno;
  size_t n = fread (out, 1, cap - 1, fp);
  int error = ferror (fp);
  int ended = feof (fp);
  fclose (fp);
  if (error)
    return -EIO;
  if (n == cap - 1 && !ended)
    return -ENOSPC;
  out[n] = 0;
  return (int)n;
#else
  return -ENOSYS;
#endif
}
#endif
