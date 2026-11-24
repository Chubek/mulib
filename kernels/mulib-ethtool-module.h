/** @file mulib-ethtool-module.h
 * @brief Retrieve transceiver SFP/module EEPROM type and size via real
 * ETHTOOL_GMODULEINFO ioctl C11, single-header, define
 * MULIB_IMPL_ETHTOOL_MODULE exactly once.
 */
#ifndef MULIB_ETHTOOL_MODULE_H
#define MULIB_ETHTOOL_MODULE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_ethtool_module_info (const char *interface, uint32_t *module_type,
                                 uint32_t *eeprom_bytes);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ETHTOOL_MODULE)                                        \
    && !defined(MULIB_ETHTOOL_MODULE_IMPLEMENTED)
#define MULIB_ETHTOOL_MODULE_IMPLEMENTED
#include <errno.h>
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
mulib_ethtool_module_info (const char *name, uint32_t *type, uint32_t *size)
{
  if (!name || !type || !size || !name[0] || strlen (name) >= 16)
    return -EINVAL;
#ifdef __linux__
  struct ethtool_modinfo info = { 0 };
  info.cmd = ETHTOOL_GMODULEINFO;
  struct ifreq ifr = { 0 };
  strncpy (ifr.ifr_name, name, sizeof ifr.ifr_name - 1);
  ifr.ifr_data = (void *)&info;
  int fd = socket (AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return -errno;
  int r = ioctl (fd, SIOCETHTOOL, &ifr);
  int e = errno;
  close (fd);
  if (r < 0)
    return -e;
  *type = info.type;
  *size = info.eeprom_len;
  return 0;
#else
  return -ENOSYS;
#endif
}
#endif
