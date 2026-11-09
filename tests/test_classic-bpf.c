/* Generated, standalone MuLIB add-on symbol and API test: classic-bpf */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_CLASSIC_BPF
#include "mulib-classic-bpf.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_classic_bpf_attach;
    assert (p);
  }
  puts ("PASS classic-bpf");
  return 0;
}
