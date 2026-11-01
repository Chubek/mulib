#define MULIB_IMPL_MIR
#define MULIB_IMPL_GSSAIR
#define MULIB_IMPL_ANFIR
#include "mulib-anfir.h"
#include <assert.h>
#define N MULIB_MIR_NONE
int main(void) {
  const mulib_mir_type_t parameters[]={MULIB_MIR_I64};
  const mulib_anfir_binding_t bindings[]={
    {{MULIB_MIR_NATIVE_PARAM,MULIB_MIR_I64,0,N,N,0,N},0,{0,0},{MULIB_MIR_VOID,MULIB_MIR_VOID}},
    {{MULIB_MIR_NATIVE_ADD,MULIB_MIR_I64,0,0,N,0,N},2,{0,42},{MULIB_MIR_VOID,MULIB_MIR_I64}}
  };
  const mulib_gssair_block_t blocks[]={{MULIB_MIR_NATIVE_RETURN,1,N,N}};
  const mulib_anfir_program_t p={bindings,{NULL,2,blocks,1,NULL,0,NULL,0,{parameters,1,MULIB_MIR_I64},NULL,0}};
  mulib_mir_native_buffer_t native={0};
  assert(mulib_anfir_lower(&p,100,&native,NULL)==MULIB_MIR_OK);
  assert(native.count==4 && native.code[1].immediate==42);
  mulib_mir_native_free(&native);return 0;
}
