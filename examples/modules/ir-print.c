#define MULIB_IMPL_IR_PRINT
#include "mulib-ir-print.h"
#include <assert.h>
int main(void) {
    mulib_ir_instruction_t code[]={{MULIB_IR_LITERAL,0,0,0,42},{MULIB_IR_RETURN,0,0,0,0}};
    char text[160];assert(mulib_ir_print(code,2,text,sizeof text));
    assert(strstr(text,"0: const r0"));puts(text);return 0;
}
