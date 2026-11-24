/** @file mulib-net-stats.h
 * @brief Read rx/tx packet and byte counters from
 * /sys/class/net/<interface>/statistics C11, single-header, define
 * MULIB_IMPL_NET_STATS exactly once.
 */
#ifndef MULIB_NET_STATS_H
#define MULIB_NET_STATS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_net_stats
  {
    uint64_t rx_bytes, tx_bytes, rx_packets, tx_packets;
  } mulib_net_stats_t;
  int mulib_net_stats_read (const char *interface, mulib_net_stats_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NET_STATS) && !defined(MULIB_NET_STATS_IMPLEMENTED)
#define MULIB_NET_STATS_IMPLEMENTED
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int
mulib_net_stats_field_ (const char *name, const char *field, uint64_t *out)
{
  char path[256];
  snprintf (path, sizeof path, "/sys/class/net/%s/statistics/%s", name, field);
  FILE *f = fopen (path, "r");
  if (!f)
    return -errno;
  unsigned long long v = 0;
  int r = fscanf (f, "%llu", &v);
  fclose (f);
  if (r != 1)
    return -EBADMSG;
  *out = (uint64_t)v;
  return 0;
}
int
mulib_net_stats_read (const char *name, mulib_net_stats_t *out)
{
  if (!name || !out || !name[0] || strlen (name) > 15)
    return -EINVAL;
  for (const char *p = name; *p; p++)
    if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
          || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' || *p == '.'))
      return -EINVAL;
#ifdef __linux__
  mulib_net_stats_t s = { 0 };
  int r;
  if ((r = mulib_net_stats_field_ (name, "rx_bytes", &s.rx_bytes))
      || (r = mulib_net_stats_field_ (name, "tx_bytes", &s.tx_bytes))
      || (r = mulib_net_stats_field_ (name, "rx_packets", &s.rx_packets))
      || (r = mulib_net_stats_field_ (name, "tx_packets", &s.tx_packets)))
    return r;
  *out = s;
  return 0;
#else
  return -ENOSYS;
#endif
}
#endif
