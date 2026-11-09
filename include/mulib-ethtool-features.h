/** @file mulib-ethtool-features.h
 * @brief Read an actual 32-bit block of kernel ethtool
 * available/requested/active feature bits C11, single-header, define
 * MULIB_IMPL_ETHTOOL_FEATURES exactly once.
 */
#ifndef MULIB_ETHTOOL_FEATURES_H
#define MULIB_ETHTOOL_FEATURES_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_ethtool_features_block
  {
    uint32_t available, requested, active, never_changed;
  } mulib_ethtool_features_block_t;
  int mulib_ethtool_features_get (const char *interface, unsigned block_index,
                                  mulib_ethtool_features_block_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ETHTOOL_FEATURES)                                      \
    && !defined(MULIB_ETHTOOL_FEATURES_IMPLEMENTED)
#define MULIB_ETHTOOL_FEATURES_IMPLEMENTED
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <linux/ethtool.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
int
mulib_ethtool_features_get (const char *name, unsigned index,
                            mulib_ethtool_features_block_t *out)
{
  if (!name || !out || !name[0] || strlen (name) >= 16 || index >= 32)
    return -EINVAL;
#ifdef __linux__
  size_t size
      = sizeof (struct ethtool_gfeatures)
        + (size_t)(index + 1) * sizeof (struct ethtool_get_features_block);
  struct ethtool_gfeatures *g = calloc (1, size);
  if (!g)
    return -ENOMEM;
  g->cmd = ETHTOOL_GFEATURES;
  g->size = index + 1;
  struct ifreq ifr = { 0 };
  strncpy (ifr.ifr_name, name, sizeof ifr.ifr_name - 1);
  ifr.ifr_data = (void *)g;
  int fd = socket (AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (fd < 0)
    {
      int e = errno;
      free (g);
      return -e;
    }
  int r = ioctl (fd, SIOCETHTOOL, &ifr);
  int e = errno;
  close (fd);
  if (r < 0)
    {
      free (g);
      return -e;
    }
  if (index >= g->size)
    {
      free (g);
      return -ENOENT;
    }
  struct ethtool_get_features_block b = g->features[index];
  free (g);
  mulib_ethtool_features_block_t v
      = { b.available, b.requested, b.active, b.never_changed };
  *out = v;
  return 0;
#else
  return -ENOSYS;
#endif
}
#endif
