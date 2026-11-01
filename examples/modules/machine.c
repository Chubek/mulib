#define MULIB_IMPL_MIR
#define MULIB_IMPL_MUBURG
#define MULIB_IMPL_MACHINE
#include "mulib-machine.h"
#include <assert.h>
int main(void) {
  mulib_mir_native_instruction_t add=mulib_mir_native_instruction(MULIB_MIR_NATIVE_ADD);
  mulib_machine_selection_t selected;
  const uint64_t operands[]={0,42};
  assert(mulib_machine_select(mulib_machine_x86_64_sysv(),&add,2,operands,100,&selected)==MULIB_MIR_OK);
  assert(selected.immediate_mask==2);
  assert(mulib_machine_select(mulib_machine_rv64im(),&add,2,operands,100,&selected)==MULIB_MIR_OK);
  assert(selected.immediate_mask==2);return 0;
}
