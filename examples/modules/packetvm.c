#define MULIB_IMPL_PACKETVM
#include "mulib-packetvm.h"
#include <assert.h>
int
main (void)
{
  const mulib_packetvm_instruction_t code[]
      = { { MULIB_PACKETVM_LD8, 0, 0, 0 },
          { MULIB_PACKETVM_AND, 0xf0, 0, 0 },
          { MULIB_PACKETVM_JEQ, 0x40, 1, 0 },
          { MULIB_PACKETVM_RET, 0, 0, 0 },
          { MULIB_PACKETVM_RET, 1, 0, 0 } };
  unsigned char packet[] = { 0x45 };
  int verdict;
  assert (!mulib_packetvm_run (code, 5, packet, sizeof packet, 5, &verdict)
          && verdict == 1);
  return 0;
}
