/** @file mulib-netfilter-log.h
 * @brief Subscribe to a Linux NFLOG group with a raw NETLINK_NETFILTER socket
 * C11, single-header, define MULIB_IMPL_NETFILTER_LOG exactly once.
 */
#ifndef MULIB_NETFILTER_LOG_H
#define MULIB_NETFILTER_LOG_H
#include "mulib-netlink-attr.h"
#include "mulib-nfnetlink.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_netfilter_log_subscribe (mulib_netlink_socket_t *s,
                                     uint16_t group);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NETFILTER_LOG)                                         \
    && !defined(MULIB_NETFILTER_LOG_IMPLEMENTED)
#define MULIB_NETFILTER_LOG_IMPLEMENTED
#include <errno.h>
#include <string.h>
#ifdef __linux__
#include <arpa/inet.h>
#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_log.h>
#endif
int
mulib_netfilter_log_subscribe (mulib_netlink_socket_t *s, uint16_t group)
{
  if (!s || s->fd != -1)
    return -EINVAL;
#ifdef __linux__
  int r = mulib_nfnetlink_open (s);
  if (r)
    return r;
  struct nfulnl_msg_config_cmd cmd = { .command = NFULNL_CFG_CMD_BIND };
  unsigned char attrs[32];
  size_t len = 0;
  r = mulib_netlink_attr_append (attrs, sizeof attrs, &len, NFULA_CFG_CMD,
                                 &cmd, sizeof cmd);
  if (!r)
    {
      struct
      {
        struct nlmsghdr h;
        struct nfgenmsg g;
        uint8_t a[32];
      } req = { 0 };
      req.h.nlmsg_len = NLMSG_LENGTH (sizeof req.g) + len;
      req.h.nlmsg_type = (NFNL_SUBSYS_ULOG << 8) | NFULNL_MSG_CONFIG;
      req.h.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
      req.g.version = NFNETLINK_V0;
      req.g.res_id = htons (group);
      memcpy (req.a, attrs, len);
      r = mulib_netlink_transact (s, &req, req.h.nlmsg_len, NULL, NULL);
    }
  if (r)
    mulib_netlink_close (s);
  return r;
#else
  return -ENOSYS;
#endif
}
#endif
