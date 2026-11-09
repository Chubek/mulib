/* Generated, standalone MuLIB add-on symbol and API test: bpf-kernel */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_BPF_KERNEL
#include "mulib-bpf-kernel.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_bpf_kernel_array_map;
    assert (p);
  }
  puts ("PASS bpf-kernel");
  return 0;
}
