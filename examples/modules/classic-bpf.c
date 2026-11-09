#define MULIB_IMPL_CLASSIC_BPF
#include "mulib-classic-bpf.h"
#include <assert.h>
#include <unistd.h>
int
main (void)
{
  int pair[2];
  assert (!socketpair (AF_UNIX, SOCK_DGRAM, 0, pair));
  const mulib_classic_bpf_insn_t accept = { 0x06, 0, 0, 0xffffffffu };
  assert (!mulib_classic_bpf_attach (pair[0], &accept, 1));
  close (pair[0]);
  close (pair[1]);
  return 0;
}
