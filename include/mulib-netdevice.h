/** @file mulib-netdevice.h
 * @brief POSIX network-interface enumeration and address snapshots.
 * Define MULIB_IMPL_NETDEVICE once. Returned callback records are borrowed.
 */
#ifndef MULIB_NETDEVICE_H
#define MULIB_NETDEVICE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_netdevice_info
  {
    const char *name;
    unsigned index, flags, family; /* family 0 (other), 4, 6 */
    unsigned char address[16], netmask[16];
    uint32_t scope;
  } mulib_netdevice_info_t;
  /** @brief Visit getifaddrs records (multiple per interface). 0 completion,
   * 1 callback stop, -1 invalid, -2 OS failure, -3 unsupported. */
  int mulib_netdevice_list (int (*visit) (void *,
                                          const mulib_netdevice_info_t *),
                            void *);
  /** @brief Resolve native interface index; zero if unknown/unsupported. */
  unsigned mulib_netdevice_index (const char *name);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NETDEVICE) && !defined(MULIB_NETDEVICE_IMPLEMENTED)
#define MULIB_NETDEVICE_IMPLEMENTED
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#endif
unsigned
mulib_netdevice_index (const char *name)
{
  if (!name || !*name)
    return 0;
#if defined(__unix__) || defined(__APPLE__)
  return if_nametoindex (name);
#else
  return 0;
#endif
}
int
mulib_netdevice_list (int (*visit) (void *, const mulib_netdevice_info_t *),
                      void *ctx)
{
  if (!visit)
    return -1;
#if defined(__unix__) || defined(__APPLE__)
  {
    struct ifaddrs *head, *p;
    int result = 0;
    if (getifaddrs (&head))
      return -2;
    for (p = head; p; p = p->ifa_next)
      {
        mulib_netdevice_info_t v;
        memset (&v, 0, sizeof (v));
        v.name = p->ifa_name;
        v.index = if_nametoindex (p->ifa_name);
        v.flags = p->ifa_flags;
        if (p->ifa_addr && p->ifa_addr->sa_family == AF_INET)
          {
            v.family = 4;
            memcpy (v.address, &((struct sockaddr_in *)p->ifa_addr)->sin_addr,
                    4);
            if (p->ifa_netmask)
              memcpy (v.netmask,
                      &((struct sockaddr_in *)p->ifa_netmask)->sin_addr, 4);
          }
        else if (p->ifa_addr && p->ifa_addr->sa_family == AF_INET6)
          {
            v.family = 6;
            v.scope = ((struct sockaddr_in6 *)p->ifa_addr)->sin6_scope_id;
            memcpy (v.address,
                    &((struct sockaddr_in6 *)p->ifa_addr)->sin6_addr, 16);
            if (p->ifa_netmask)
              memcpy (v.netmask,
                      &((struct sockaddr_in6 *)p->ifa_netmask)->sin6_addr, 16);
          }
        if (visit (ctx, &v))
          {
            result = 1;
            break;
          }
      }
    freeifaddrs (head);
    return result;
  }
#else
  (void)ctx;
  return -3;
#endif
}
#endif
