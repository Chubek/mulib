#define MULIB_IMPL_BYTECODE_DISASM
#include "mulib-bytecode-disasm.h"
#include <assert.h>
int main(void) {
    mulib_bc_instruction_t code[]={{MULIB_BC_PUSH,42},{MULIB_BC_HALT,0}};
    char text[80];
    assert(mulib_bytecode_disassemble(code,2,text,sizeof text));
    assert(!strcmp(text,"0: PUSH 42\n1: HALT 0\n"));
    puts(text);return 0;
}
