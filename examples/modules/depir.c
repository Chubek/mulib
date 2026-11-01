#define MULIB_IMPL_MIR
#define MULIB_IMPL_GSSAIR
#define MULIB_IMPL_CLICKIR
#define MULIB_IMPL_ANFIR
#define MULIB_IMPL_SUPERBLOCKS
#define MULIB_IMPL_DEPIR
#include "mulib-depir.h"
#include <assert.h>
#define N MULIB_MIR_NONE
int main(void) {
  const mulib_gssair_node_t nodes[]={{MULIB_MIR_NATIVE_CONST,MULIB_MIR_I64,0,N,N,42,N}};
  const mulib_gssair_block_t blocks[]={{MULIB_MIR_NATIVE_RETURN,0,N,N}};
  const mulib_gssair_program_t p={nodes,1,blocks,1,NULL,0,NULL,0,{NULL,0,MULIB_MIR_I64},NULL,0};
  mulib_depir_buffer_t dependencies={0};mulib_anfir_buffer_t anf={0};
  mulib_mir_native_buffer_t native={0};
  assert(mulib_depir_from_gssair(&p,100,&dependencies,NULL)==MULIB_MIR_OK);
  assert(dependencies.program.entity_count==6);
  assert(mulib_depir_to_anfir(&dependencies.program,100,&anf,NULL)==MULIB_MIR_OK);
  assert(mulib_depir_lower(&dependencies.program,100,&native,NULL)==MULIB_MIR_OK);
  mulib_mir_native_free(&native);mulib_anfir_free(&anf);mulib_depir_free(&dependencies);return 0;
}
