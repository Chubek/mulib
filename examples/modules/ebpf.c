#define MULIB_IMPL_EBPF
#include "mulib-ebpf.h"
#include <assert.h>
int
main (void)
{
  mulib_ebpf_instruction_t code[]
      = { { 0xb7, 0, 0, 0, 40 }, { 0x07, 0, 0, 0, 2 }, { 0x95, 0, 0, 0, 0 } };
  uint64_t value;
  assert (!mulib_ebpf_run (code, 3, 3, &value) && value == 42);
  return 0;
}
