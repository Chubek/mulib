/* Optimizer implementation is independent of the interpreter implementation. */
#define MULIB_IMPL_BYTECODE_OPT
#include "mulib-bytecode-opt.h"
#include <assert.h>
int main(void) {
    mulib_bc_instruction_t code[]={{MULIB_BC_PUSH,6},{MULIB_BC_PUSH,7},{MULIB_BC_MUL,0},{MULIB_BC_HALT,0}};
    size_t count=4,folded;
    assert(mulib_bytecode_optimize(code,&count,&folded));
    assert(count==2&&folded==1&&code[0].operand==42);
    return 0;
}
