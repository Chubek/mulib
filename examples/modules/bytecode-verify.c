#define MULIB_IMPL_BYTECODE_VERIFY
#include "mulib-bytecode-verify.h"
#include <assert.h>
int main(void) {
    mulib_bc_instruction_t code[]={{MULIB_BC_PUSH,1},{MULIB_BC_JZ,4},{MULIB_BC_PUSH,42},
        {MULIB_BC_JMP,5},{MULIB_BC_PUSH,0},{MULIB_BC_HALT,0}};
    size_t peak,bad;
    assert(mulib_bytecode_verify(code,6,0,&peak,&bad)&&peak==1&&bad==SIZE_MAX);
    return 0;
}
