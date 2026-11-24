/** @file mulib-classic-bpf.h
 * @brief Attach Linux classic socket BPF instruction arrays to an existing
 * socket C11, single-header, define MULIB_IMPL_CLASSIC_BPF exactly once.
 */
#ifndef MULIB_CLASSIC_BPF_H
#define MULIB_CLASSIC_BPF_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_classic_bpf_insn
  {
    uint16_t code;
    uint8_t jt, jf;
    uint32_t k;
  } mulib_classic_bpf_insn_t;
  int mulib_classic_bpf_attach (int fd, const mulib_classic_bpf_insn_t *insns,
                                size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CLASSIC_BPF) && !defined(MULIB_CLASSIC_BPF_IMPLEMENTED)
#define MULIB_CLASSIC_BPF_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <asm/socket.h>
#include <linux/filter.h>
#include <sys/socket.h>
#endif
int
mulib_classic_bpf_attach (int fd, const mulib_classic_bpf_insn_t *ins,
                          size_t n)
{
  if (fd < 0 || !ins || !n || n > 65535)
    return -EINVAL;
#ifdef __linux__
  struct sock_fprog p
      = { (unsigned short)n, (struct sock_filter *)(void *)ins };
  return setsockopt (fd, SOL_SOCKET, SO_ATTACH_FILTER, &p, sizeof p) < 0
             ? -errno
             : 0;
#else
  return -ENOSYS;
#endif
}
#endif
