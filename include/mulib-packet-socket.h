/** @file mulib-packet-socket.h
 * @brief Open, bind, and receive Linux AF_PACKET raw Ethernet frames
 * C11, single-header, define MULIB_IMPL_PACKET_SOCKET exactly once.
 */
#ifndef MULIB_PACKET_SOCKET_H
#define MULIB_PACKET_SOCKET_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_packet_socket_open (unsigned ifindex, uint16_t ethertype);
  int mulib_packet_socket_receive (int fd, void *buffer, size_t capacity,
                                   size_t *received);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PACKET_SOCKET)                                         \
    && !defined(MULIB_PACKET_SOCKET_IMPLEMENTED)
#define MULIB_PACKET_SOCKET_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
int
mulib_packet_socket_open (unsigned idx, uint16_t protocol)
{
  if (!idx)
    return -EINVAL;
#ifdef __linux__
  int fd = socket (AF_PACKET, SOCK_RAW | SOCK_CLOEXEC,
                   htons (protocol ? protocol : ETH_P_ALL));
  if (fd < 0)
    return -errno;
  struct sockaddr_ll a = { 0 };
  a.sll_family = AF_PACKET;
  a.sll_protocol = htons (protocol ? protocol : ETH_P_ALL);
  a.sll_ifindex = (int)idx;
  if (bind (fd, (void *)&a, sizeof a) < 0)
    {
      int e = errno;
      close (fd);
      return -e;
    }
  return fd;
#else
  return -ENOSYS;
#endif
}
int
mulib_packet_socket_receive (int fd, void *p, size_t cap, size_t *got)
{
  if (fd < 0 || !p || !got || !cap)
    return -EINVAL;
  *got = 0;
#ifdef __linux__
  ssize_t n = recv (fd, p, cap, MSG_TRUNC);
  if (n < 0)
    return -errno;
  if ((size_t)n > cap)
    return -EMSGSIZE;
  *got = (size_t)n;
  return 0;
#else
  return -ENOSYS;
#endif
}
#endif
