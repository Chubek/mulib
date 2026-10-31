/* Exercise every allocation failure in image copying, optimization, frame
 * construction and native compilation, including partially copied signatures. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static size_t fail_at, calls, live;
static void *checked_malloc(size_t n)
{
    void *p;
    if (++calls == fail_at) return NULL;
    p = malloc(n);
    if (p) ++live;
    return p;
}
static void *checked_calloc(size_t count, size_t n)
{
    void *p;
    assert(!n || count <= SIZE_MAX / n);
    p = checked_malloc(count * n);
    if (p) memset(p,0,count*n);
    return p;
}
static void checked_free(void *p)
{
    if (p) { assert(live); --live; }
    free(p);
}
#define malloc checked_malloc
#define calloc checked_calloc
#define free checked_free
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#include "mulib-jit.h"
#undef malloc
#undef calloc
#undef free

#define N MULIB_MIR_NONE
static const mulib_mir_type_t types[] = {MULIB_MIR_I64,MULIB_MIR_I64};
static const mulib_mir_flow_instruction_t code[] = {
    {MULIB_MIR_FLOW_PARAM,0,0,N,N,{.word=0}},
    {MULIB_MIR_FLOW_CONST,1,N,N,N,{.integer=0}},
    {MULIB_MIR_FLOW_ADD,0,0,1,N,{.word=0}},
    {MULIB_MIR_FLOW_CALL,1,0,1,0,{.word=0}},
    {MULIB_MIR_FLOW_RETURN,N,1,N,N,{.word=0}}
};
static const mulib_mir_signature_t hosts[] = {
    {types,1,MULIB_MIR_I64}, {types,2,MULIB_MIR_BOOL}
};
static const mulib_mir_flow_program_t program = {
    code,5,types,2,{types,1,MULIB_MIR_I64},hosts,2
};
static mulib_mir_status_t callback(void *user, size_t h,
    const mulib_mir_value_t *args, size_t n, mulib_mir_value_t *out)
{
    (void)user; assert(h==0 && n==1); *out=args[0]; return MULIB_MIR_OK;
}
int main(void)
{
    size_t k;
    int succeeded = 0;
    for(k=1;k<32;++k) {
        mulib_vmweave_program_t *vm=NULL;
        mulib_mir_diagnostic_t d;
        mulib_mir_status_t s;
        calls=0; fail_at=k;
        s=mulib_vmweave_compile(&program,100,1,&vm,&d);
        if(s==MULIB_MIR_OK) {
            succeeded=1; assert(vm); mulib_vmweave_program_destroy(vm);
        } else assert(s==MULIB_MIR_NO_MEMORY && !vm && d.status==s);
        assert(!live);
        if(succeeded) break;
    }
    assert(succeeded && k>=8);
    succeeded=0;
    for(k=1;k<8;++k) {
        mulib_mir_frame_t f={0};
        mulib_mir_value_t arg={.integer=42};
        mulib_mir_status_t s;
        mulib_mir_diagnostic_t d;
        calls=0; fail_at=k;
        s=mulib_mir_frame_init(&f,&program,&arg,1,callback,NULL,100,&d);
        if(s==MULIB_MIR_OK) {
            succeeded=1;
            assert(mulib_vmweave_run(&f,10)==MULIB_MIR_FINISHED && f.result.integer==42);
            mulib_mir_frame_free(&f);
        } else assert(s==MULIB_MIR_NO_MEMORY && !f.program && !f.registers && !f.arguments && d.status==s);
        assert(!live);
        if(succeeded) break;
    }
    assert(succeeded && k==3);
    if(mulib_jit_supported()) {
        succeeded=0;
        for(k=1;k<32;++k) {
            mulib_jit_flow_t *jit=NULL;
            int s;
            calls=0; fail_at=k;
            s=mulib_jit_flow_compile(&program,100,1,&jit,NULL);
            if(s==MULIB_JIT_OK) {
                succeeded=1; assert(jit); assert(mulib_jit_flow_destroy(jit)==0);
            } else assert(!jit && (s==MULIB_JIT_EMIR || s==MULIB_JIT_ENOMEM));
            assert(!live);
            if(succeeded) break;
        }
        assert(succeeded && k>=10);
    }
    return 0;
}
