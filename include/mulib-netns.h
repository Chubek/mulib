/** @file mulib-netns.h
 * @brief Open and enter Linux network namespaces explicitly
 * C11, single-header, define MULIB_IMPL_NETNS exactly once.
 */
#ifndef MULIB_NETNS_H
#define MULIB_NETNS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_netns_open_self (void);
  int mulib_netns_open_pid (int pid);
  int mulib_netns_enter (int nsfd);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NETNS) && !defined(MULIB_NETNS_IMPLEMENTED)
#define MULIB_NETNS_IMPLEMENTED
#include <errno.h>
#include <stdio.h>
#ifdef __linux__
#include <fcntl.h>
#include <linux/sched.h>
#include <sched.h>
#include <unistd.h>
extern int setns (int fd, int nstype);
#ifndef O_CLOEXEC
#define MULIB_NETNS_CLOEXEC_ 02000000
#else
#define MULIB_NETNS_CLOEXEC_ O_CLOEXEC
#endif
#endif
int
mulib_netns_open_self (void)
{
#ifdef __linux__
  int fd = open ("/proc/self/ns/net", O_RDONLY | MULIB_NETNS_CLOEXEC_);
  return fd < 0 ? -errno : fd;
#else
  return -ENOSYS;
#endif
}
int
mulib_netns_open_pid (int pid)
{
  if (pid <= 0)
    return -EINVAL;
#ifdef __linux__
  char path[64];
  snprintf (path, sizeof path, "/proc/%d/ns/net", pid);
  int fd = open (path, O_RDONLY | MULIB_NETNS_CLOEXEC_);
  return fd < 0 ? -errno : fd;
#else
  return -ENOSYS;
#endif
}
int
mulib_netns_enter (int fd)
{
  if (fd < 0)
    return -EINVAL;
#ifdef __linux__
  return setns (fd, CLONE_NEWNET) < 0 ? -errno : 0;
#else
  return -ENOSYS;
#endif
}
#endif
