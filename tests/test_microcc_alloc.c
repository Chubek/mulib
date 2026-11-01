#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static size_t fail_at,calls,live;
static void *checked_malloc(size_t n) {
  void *p;if(++calls==fail_at)return NULL;p=malloc(n);if(p)++live;return p;
}
static void *checked_calloc(size_t n,size_t size) {
  void *p;assert(!size || n<=SIZE_MAX/size);p=checked_malloc(n*size);if(p)memset(p,0,n*size);return p;
}
static void *checked_realloc(void *old,size_t n) {
  void *p;int had=old!=NULL;assert(n);if(++calls==fail_at)return NULL;p=realloc(old,n);if(p&&!had)++live;return p;
}
static void checked_free(void *p) { if(p){assert(live);--live;}free(p); }
#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#define free checked_free
#define MULIB_IMPL_MIR
#define MULIB_IMPL_GSSAIR
#define MULIB_IMPL_CLICKIR
#define MULIB_IMPL_ANFIR
#define MULIB_IMPL_SUPERBLOCKS
#define MULIB_IMPL_DEPIR
#define MULIB_IMPL_MUBURG
#define MULIB_IMPL_MACHINE
#define MULIB_IMPL_ISCHED
#define MULIB_IMPL_LOWOPT
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_MICROCC
#include "mulib-microcc.h"
#undef malloc
#undef calloc
#undef realloc
#undef free
#define N MULIB_MIR_NONE
static const mulib_mir_type_t types[]={MULIB_MIR_WORD,MULIB_MIR_I64};
static const mulib_gssair_function_t funcs[]={{"callee",{types,2,MULIB_MIR_I64}}};
static const mulib_gssair_node_t nodes[]={
  {MULIB_MIR_NATIVE_PARAM,MULIB_MIR_WORD,0,N,N,0,N},
  {MULIB_MIR_NATIVE_CONST,MULIB_MIR_I64,0,N,N,7,N},
  {MULIB_MIR_NATIVE_CALL,MULIB_MIR_I64,0,0,2,0,N},
  {MULIB_MIR_NATIVE_LOAD,MULIB_MIR_I64,0,0,N,0,2},
  {MULIB_MIR_NATIVE_ADD,MULIB_MIR_I64,0,2,3,0,N}
};
static const size_t args[]={0,1};
static const mulib_gssair_block_t blocks[]={{MULIB_MIR_NATIVE_RETURN,4,N,N}};
static const mulib_gssair_program_t graph={nodes,5,blocks,1,NULL,0,args,2,{types,1,MULIB_MIR_I64},funcs,1};
int main(void) {
  size_t action,k;
  /* Fail every allocation of each structural conversion, including partial
   * construction. All owners must remain empty and no allocation may leak. */
  for(action=0;action<5;++action) {
    int success=0;
    for(k=1;k<1000;++k) {
      mulib_depir_buffer_t dep={0};mulib_anfir_buffer_t anf={0};mulib_superblocks_buffer_t super={0};
      mulib_clickir_buffer_t click={0};mulib_mir_native_buffer_t native={0};mulib_mir_status_t s;
      calls=0;fail_at=k;
      switch(action) {
      case 0:s=mulib_depir_from_gssair(&graph,256,&dep,NULL);break;
      case 1:s=mulib_anfir_from_gssair(&graph,256,&anf,NULL);break;
      case 2:s=mulib_superblocks_build(&graph,256,&super,NULL);break;
      case 3:s=mulib_clickir_from_gssair(&graph,256,&click,NULL);break;
      default:s=mulib_gssair_lower(&graph,256,&native,NULL);break;
      }
      if(s==MULIB_MIR_OK) success=1;
      else assert(s==MULIB_MIR_NO_MEMORY && !dep.entities && !anf.bindings && !super.order && !click.graph.nodes && !native.code);
      mulib_depir_free(&dep);mulib_anfir_free(&anf);mulib_superblocks_free(&super);
      mulib_clickir_free(&click);mulib_mir_native_free(&native);assert(!live);
      if(success)break;
    }
    assert(success && k>5);
  }
  {
    mulib_depir_buffer_t dep={0};mulib_anfir_buffer_t anf={0};mulib_superblocks_buffer_t super={0};
    mulib_clickir_buffer_t click={0};mulib_mir_native_buffer_t native={0};mulib_mir_native_program_t np;
    mulib_microcc_source_t sources[6];size_t baseline;
    fail_at=0;
    assert(mulib_depir_from_gssair(&graph,256,&dep,NULL)==MULIB_MIR_OK);
    assert(mulib_anfir_from_gssair(&graph,256,&anf,NULL)==MULIB_MIR_OK);
    assert(mulib_superblocks_build(&graph,256,&super,NULL)==MULIB_MIR_OK);
    assert(mulib_clickir_from_gssair(&graph,256,&click,NULL)==MULIB_MIR_OK);
    assert(mulib_gssair_lower(&graph,256,&native,NULL)==MULIB_MIR_OK);np=mulib_mir_native_view(&native);
    sources[0]=(mulib_microcc_source_t){MULIB_MICROCC_GSSAIR,&graph};
    sources[1]=(mulib_microcc_source_t){MULIB_MICROCC_DEPIR,&dep.program};
    sources[2]=(mulib_microcc_source_t){MULIB_MICROCC_ANFIR,&anf.program};
    sources[3]=(mulib_microcc_source_t){MULIB_MICROCC_SUPERBLOCKS,&super.program};
    sources[4]=(mulib_microcc_source_t){MULIB_MICROCC_CLICKIR,&click.program};
    sources[5]=(mulib_microcc_source_t){MULIB_MICROCC_NATIVE,&np};baseline=live;
    for(action=0;action<6;++action) {
      int success=0;
      for(k=1;k<2000;++k) {
        mulib_microcc_result_t r={0};mulib_microcc_diagnostic_t d;mulib_mir_status_t s;
        calls=0;fail_at=k;s=mulib_microcc_compile(&sources[action],"compiled",NULL,&r,&d);
        if(s==MULIB_MIR_OK)success=1;
        else assert(s==MULIB_MIR_NO_MEMORY && d.mir.status==s && !r.mir.code && !r.selected && !r.assembly);
        mulib_microcc_free(&r);assert(live==baseline);if(success)break;
      }
      assert(success && k>40);
    }
    mulib_depir_free(&dep);mulib_anfir_free(&anf);mulib_superblocks_free(&super);
    mulib_clickir_free(&click);mulib_mir_native_free(&native);assert(!live);
  }
  return 0;
}
