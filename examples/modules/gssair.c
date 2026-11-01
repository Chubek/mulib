#define MULIB_IMPL_MIR
#define MULIB_IMPL_GSSAIR
#include "mulib-gssair.h"
#include <assert.h>
#define N MULIB_MIR_NONE
int main(void) {
  const mulib_gssair_node_t nodes[]={
    {MULIB_MIR_NATIVE_CONST,MULIB_MIR_I64,0,N,N,42,N}
  };
  const mulib_gssair_block_t blocks[]={{MULIB_MIR_NATIVE_RETURN,0,N,N}};
  const mulib_gssair_program_t p={nodes,1,blocks,1,NULL,0,NULL,0,{NULL,0,MULIB_MIR_I64},NULL,0};
  mulib_mir_native_buffer_t native={0};
  assert(mulib_gssair_validate(&p,100,NULL)==MULIB_MIR_OK);
  assert(mulib_gssair_lower(&p,100,&native,NULL)==MULIB_MIR_OK);
  assert(native.count==2 && native.code[0].immediate==42);
  mulib_mir_native_free(&native);return 0;
}
