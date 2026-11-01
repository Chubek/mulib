#define MULIB_IMPL_MIR
#define MULIB_IMPL_LOWOPT
#include "mulib-lowopt.h"
#include <assert.h>
#define N MULIB_MIR_NONE
int main(void) {
  const mulib_mir_native_instruction_t code[]={
    {MULIB_MIR_NATIVE_CONST,0,N,N,N,6},{MULIB_MIR_NATIVE_CONST,1,N,N,N,7},
    {MULIB_MIR_NATIVE_MUL,2,0,1,N,0},{MULIB_MIR_NATIVE_RETURN,N,2,N,N,0}
  };
  const mulib_mir_native_block_t block={0,4};
  const mulib_mir_native_program_t p={code,4,&block,1,3,0,NULL,0,NULL,0};
  mulib_mir_native_buffer_t optimized={0};mulib_lowopt_stats_t stats;
  assert(mulib_lowopt_run(&p,100,&optimized,&stats,NULL)==MULIB_MIR_OK);
  assert(stats.folded==1 && stats.removed==2 && optimized.code[2].immediate==42);
  mulib_mir_native_free(&optimized);return 0;
}
