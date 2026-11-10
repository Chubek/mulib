#define MULIB_IMPL_BYTECODE
#define MULIB_IMPL_BYTECODE_VERIFY
#define MULIB_IMPL_BYTECODE_EVAL
#include "mulib-bytecode-eval.h"
#include <assert.h>
int main(void) {
    mulib_bc_instruction_t code[]={{MULIB_BC_PUSH,42},{MULIB_BC_HALT,0}};
    mulib_bc_result_t result;
    assert(mulib_bytecode_eval(code,2,0,2,&result)&&result.value==42);
    return 0;
}
