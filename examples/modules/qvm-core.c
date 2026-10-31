#include "include-mulib-qvm.h"
#include <assert.h>
int main (void)
{
  const mulib_qvm_instruction code[] = {
    { MULIB_QVM_CONST, 0, 0, 0, 5 }, { MULIB_QVM_HALT, 0, 0, 0, 0 }
  };
  mulib_qvm vm;
  assert (!mulib_qvm_init (&vm, code, 2, 64, 4096, 100, NULL, NULL));
  assert (mulib_qvm_run (&vm, 1) == MULIB_CT_AGAIN);
  assert (mulib_qvm_run (&vm, 1) == MULIB_CT_DONE && vm.exit_code == 5);
  mulib_qvm_free (&vm);
  return 0;
}
