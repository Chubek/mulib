/** @file mulib-net-state.h
 * @brief Snapshot interface state from sysfs without modifying the network
 * C11, single-header, define MULIB_IMPL_NET_STATE exactly once.
 */
#ifndef MULIB_NET_STATE_H
#define MULIB_NET_STATE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_net_state
  {
    unsigned index;
    uint32_t mtu;
    int oper_up;
  } mulib_net_state_t;
  int mulib_net_state_read (const char *interface, mulib_net_state_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NET_STATE) && !defined(MULIB_NET_STATE_IMPLEMENTED)
#define MULIB_NET_STATE_IMPLEMENTED
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef __linux__
#include <net/if.h>
#endif
int
mulib_net_state_read (const char *name, mulib_net_state_t *out)
{
  if (!name || !out || strlen (name) > 15 || !name[0])
    return -EINVAL;
  for (const char *p = name; *p; p++)
    if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
          || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' || *p == '.'))
      return -EINVAL;
#ifdef __linux__
  char path[128], state[32];
  unsigned mtu = 0;
  int n = snprintf (path, sizeof path, "/sys/class/net/%s/mtu", name);
  if (n < 0 || (size_t)n >= sizeof path)
    return -ENAMETOOLONG;
  FILE *f = fopen (path, "r");
  if (!f)
    return -errno;
  int ok = fscanf (f, "%u", &mtu);
  fclose (f);
  if (ok != 1)
    return -EBADMSG;
  snprintf (path, sizeof path, "/sys/class/net/%s/operstate", name);
  f = fopen (path, "r");
  if (!f)
    return -errno;
  ok = fscanf (f, "%31s", state);
  fclose (f);
  if (ok != 1)
    return -EBADMSG;
  unsigned idx = if_nametoindex (name);
  if (!idx)
    return -ENODEV;
  mulib_net_state_t v = { idx, mtu, !strcmp (state, "up") };
  *out = v;
  return 0;
#else
  return -ENOSYS;
#endif
}
#endif
