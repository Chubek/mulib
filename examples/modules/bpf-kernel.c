/* Actual map creation needs kernel policy/privileges; no map is created. */
#define MULIB_IMPL_BPF_KERNEL
#include "mulib-bpf-kernel.h"
#include <assert.h>
int
main (void)
{
  assert (mulib_bpf_kernel_array_map (0, 1) == -EINVAL);
  return 0;
}
