/** @file mulib-bpf-kernel.h
 * @brief Invoke Linux BPF_MAP_CREATE for an ARRAY map using the kernel bpf
 * syscall C11, single-header, define MULIB_IMPL_BPF_KERNEL exactly once.
 */
#ifndef MULIB_BPF_KERNEL_H
#define MULIB_BPF_KERNEL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_bpf_kernel_array_map (uint32_t value_size, uint32_t entry_count);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BPF_KERNEL) && !defined(MULIB_BPF_KERNEL_IMPLEMENTED)
#define MULIB_BPF_KERNEL_IMPLEMENTED
#include <errno.h>
#include <string.h>
#ifdef __linux__
#include <linux/bpf.h>
#include <sys/syscall.h>
#include <unistd.h>
/* glibc hides syscall in strict ISO C mode. */
extern long syscall (long number, ...);
#endif
int
mulib_bpf_kernel_array_map (uint32_t value_size, uint32_t count)
{
  if (!value_size || value_size > 65536 || !count)
    return -EINVAL;
#ifdef __linux__
  union bpf_attr a;
  memset (&a, 0, sizeof a);
  a.map_type = BPF_MAP_TYPE_ARRAY;
  a.key_size = 4;
  a.value_size = value_size;
  a.max_entries = count;
  int fd = (int)syscall (__NR_bpf, BPF_MAP_CREATE, &a, sizeof a);
  return fd < 0 ? -errno : fd;
#else
  return -ENOSYS;
#endif
}
#endif
