#define MULIB_IMPL_BYTECODE_BUILDER
#define MULIB_IMPL_BYTECODE
#include "mulib-bytecode-builder.h"
#include <assert.h>
int main(void) {
    mulib_bytecode_builder_t builder={0};size_t index;
    assert(mulib_bytecode_builder_add(&builder,MULIB_BC_PUSH,0,&index));
    assert(mulib_bytecode_builder_patch(&builder,index,42));
    assert(mulib_bytecode_builder_add(&builder,MULIB_BC_HALT,0,NULL));
    mulib_bc_result_t result;
    assert(mulib_bytecode_run(builder.code,builder.count,0,1,2,&result)==MULIB_BC_OK&&result.value==42);
    mulib_bytecode_builder_free(&builder);return 0;
}
