/* cc -std=c11 -Iinclude examples/modules/microcc.c -lm -o microcc
 * ./microcc > add42.s; cc -c add42.s
 * ./microcc rv64 > add42-rv64.s (assemble with a RISC-V toolchain). */
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
#include <stdio.h>
#include <string.h>
#define N MULIB_MIR_NONE
static int write_assembly(void *user,const char *bytes,size_t count) {
  return fwrite(bytes,1,count,(FILE *)user)==count;
}
int main(int argc,char **argv) {
  const mulib_mir_type_t parameters[]={MULIB_MIR_I64};
  const mulib_anfir_binding_t bindings[]={
    {{MULIB_MIR_NATIVE_PARAM,MULIB_MIR_I64,0,N,N,0,N},0,{0,0},{MULIB_MIR_VOID,MULIB_MIR_VOID}},
    {{MULIB_MIR_NATIVE_ADD,MULIB_MIR_I64,0,0,N,0,N},2,{0,42},{MULIB_MIR_VOID,MULIB_MIR_I64}}
  };
  const mulib_gssair_block_t blocks[]={{MULIB_MIR_NATIVE_RETURN,1,N,N}};
  const mulib_anfir_program_t program={bindings,{NULL,2,blocks,1,NULL,0,NULL,0,{parameters,1,MULIB_MIR_I64},NULL,0}};
  const mulib_microcc_source_t source={MULIB_MICROCC_ANFIR,&program};
  mulib_microcc_options_t options=mulib_microcc_defaults();
  mulib_microcc_result_t result={0};mulib_microcc_diagnostic_t diagnostic;
  mulib_mir_status_t status;
  if(argc==2 && !strcmp(argv[1],"rv64")) options.machine=mulib_machine_rv64im();
  status=mulib_microcc_compile(&source,"mulib_generated_add42",&options,&result,&diagnostic);
  if(status!=MULIB_MIR_OK) {
    fprintf(stderr,"%s: %s at %zu\n",mulib_microcc_stage_string(diagnostic.stage),
      mulib_mir_status_string(status),diagnostic.mir.record);return 1;
  }
  status=mulib_microcc_write(&result,write_assembly,stdout);
  mulib_microcc_free(&result);return status==MULIB_MIR_OK?0:1;
}
