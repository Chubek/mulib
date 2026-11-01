#define MULIB_IMPL_MIR
#define MULIB_IMPL_GSSAIR
#define MULIB_IMPL_SUPERBLOCKS
#include "mulib-superblocks.h"
#include <assert.h>
#define N MULIB_MIR_NONE
int main(void) {
  const mulib_gssair_node_t nodes[]={
    {MULIB_MIR_NATIVE_CONST,MULIB_MIR_BOOL,0,N,N,1,N},
    {MULIB_MIR_NATIVE_CONST,MULIB_MIR_I64,1,N,N,42,N},
    {MULIB_MIR_NATIVE_CONST,MULIB_MIR_I64,2,N,N,0,N}
  };
  const mulib_gssair_block_t blocks[]={
    {MULIB_MIR_NATIVE_BRANCH,0,1,2},{MULIB_MIR_NATIVE_RETURN,1,N,N},{MULIB_MIR_NATIVE_RETURN,2,N,N}
  };
  const mulib_gssair_program_t p={nodes,3,blocks,3,NULL,0,NULL,0,{NULL,0,MULIB_MIR_I64},NULL,0};
  mulib_superblocks_buffer_t traces={0};mulib_mir_native_buffer_t native={0};
  assert(mulib_superblocks_build(&p,100,&traces,NULL)==MULIB_MIR_OK);
  assert(traces.program.trace_count==2 && traces.starts[1]==2);
  assert(mulib_superblocks_lower(&traces.program,100,&native,NULL)==MULIB_MIR_OK);
  mulib_mir_native_free(&native);mulib_superblocks_free(&traces);return 0;
}
