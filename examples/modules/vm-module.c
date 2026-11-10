#define MULIB_IMPL_BYTECODE
#define MULIB_IMPL_BYTECODE_VERIFY
#define MULIB_IMPL_VM_MODULE
#include "mulib-vm-module.h"
#include <assert.h>
int main(void) {
    mulib_vm_modules_t modules={0};mulib_bc_instruction_t code[]={{MULIB_BC_PUSH,42},{MULIB_BC_HALT,0}};
    assert(mulib_vm_module_add(&modules,"main",code,2,0));
    const mulib_vm_module_t *module=mulib_vm_module_find(&modules,"main");mulib_bc_result_t result;
    assert(module&&mulib_bytecode_run(module->code,module->count,module->locals,1,2,&result)==MULIB_BC_OK);
    assert(result.value==42);mulib_vm_modules_free(&modules);return 0;
}
