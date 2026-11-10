#define MULIB_IMPL_BYTECODE_SERIALIZE
#include "mulib-bytecode-serialize.h"
#include <assert.h>
int main(void) {
    mulib_bc_instruction_t code[]={{MULIB_BC_PUSH,-42},{MULIB_BC_HALT,0}},*decoded=NULL;
    unsigned char *bytes=NULL;size_t length,count;
    assert(mulib_bytecode_encode(code,2,&bytes,&length));
    assert(mulib_bytecode_decode(bytes,length,&decoded,&count));
    assert(count==2&&decoded[0].operand==-42);
    free(bytes);free(decoded);return 0;
}
