#define MULIB_IMPL_MIR
#define MULIB_IMPL_GSSAIR
#define MULIB_IMPL_CLICKIR
#include "mulib-clickir.h"
#include <assert.h>
#define N MULIB_MIR_NONE
int main(void) {
  const mulib_gssair_node_t nodes[]={
    {MULIB_MIR_NATIVE_ADD,MULIB_MIR_I64,0,1,2,0,N},
    {MULIB_MIR_NATIVE_CONST,MULIB_MIR_I64,0,N,N,20,N},
    {MULIB_MIR_NATIVE_CONST,MULIB_MIR_I64,0,N,N,22,N}
  };
  const mulib_gssair_block_t blocks[]={{MULIB_MIR_NATIVE_RETURN,0,N,N}};
  const mulib_clickir_program_t p={{nodes,3,blocks,1,NULL,0,NULL,0,{NULL,0,MULIB_MIR_I64},NULL,0},NULL,0};
  mulib_gssair_buffer_t ordered={0};mulib_mir_native_buffer_t native={0};
  assert(mulib_clickir_to_gssair(&p,100,&ordered,NULL)==MULIB_MIR_OK);
  assert(ordered.nodes[2].opcode==MULIB_MIR_NATIVE_ADD);
  assert(mulib_clickir_lower(&p,100,&native,NULL)==MULIB_MIR_OK);
  mulib_mir_native_free(&native);mulib_gssair_free(&ordered);return 0;
}
