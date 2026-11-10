#define MULIB_IMPL_BYTECODE
#include "mulib-bytecode.h"
#include <assert.h>
int main(void) {
    mulib_bc_instruction_t code[]={{MULIB_BC_PUSH,6},{MULIB_BC_PUSH,7},{MULIB_BC_MUL,0},{MULIB_BC_HALT,0}};
    mulib_bc_result_t result;
    assert(mulib_bytecode_run(code,4,0,2,4,&result)==MULIB_BC_OK);
    assert(result.value==42);
    return 0;
}
