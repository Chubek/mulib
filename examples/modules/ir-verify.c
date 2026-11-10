#define MULIB_IMPL_IR_VERIFY
#include "mulib-ir-verify.h"
#include <assert.h>
int main(void) {
    mulib_ir_instruction_t code[]={{MULIB_IR_LITERAL,0,0,0,42},{MULIB_IR_RETURN,0,0,0,0}};
    size_t bad;assert(mulib_ir_verify(code,2,1,&bad)&&bad==SIZE_MAX);
    return 0;
}
