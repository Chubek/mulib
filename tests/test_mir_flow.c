#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#include "mulib-jit.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define N MULIB_MIR_NONE
#define INS(op,d,a,b,t) {MULIB_MIR_FLOW_##op,d,a,b,t,{.word=0}}
#define LIT(d,n) {MULIB_MIR_FLOW_CONST,d,N,N,N,{.integer=(n)}}
#define FLIT(d,n) {MULIB_MIR_FLOW_CONST,d,N,N,N,{.number=(n)}}

static const mulib_mir_type_t ints[] = {MULIB_MIR_I64, MULIB_MIR_I64};
static const mulib_mir_type_t factorial_types[] = {
    MULIB_MIR_I64, MULIB_MIR_I64, MULIB_MIR_I64, MULIB_MIR_BOOL
};
static const mulib_mir_flow_instruction_t factorial_code[] = {
    INS(PARAM,0,0,N,N), LIT(1,1), LIT(2,1), INS(LT,3,2,0,N),
    INS(BRANCH,N,3,N,6), INS(RETURN,N,1,N,N),
    INS(MUL,1,1,0,N), INS(SUB,0,0,2,N), INS(JUMP,N,N,N,3)
};
static const mulib_mir_flow_program_t factorial = {
    factorial_code, 9, factorial_types, 4, {ints,1,MULIB_MIR_I64}, NULL,0
};

static void run_factorial(void)
{
    int optimize;
    for (optimize = 0; optimize < 2; ++optimize) {
        mulib_vmweave_program_t *vm = NULL;
        mulib_jit_flow_t *jit = NULL;
        int64_t expected = 1, n;
        assert(mulib_vmweave_compile(&factorial,100,optimize,&vm,NULL) == MULIB_MIR_OK);
        if (mulib_jit_supported())
            assert(mulib_jit_flow_compile(&factorial,100,optimize,&jit,NULL) == MULIB_JIT_OK);
        for (n = 0; n <= 21; ++n) {
            mulib_mir_frame_t a = {0}, b = {0};
            mulib_mir_value_t arg = {.integer=n};
            mulib_mir_status_t s, t;
            assert(mulib_mir_frame_init(&a,mulib_vmweave_program_view(vm),&arg,1,NULL,NULL,100,NULL) == MULIB_MIR_OK);
            assert(mulib_vmweave_run(&a,0) == MULIB_MIR_STEP_LIMIT && !a.steps);
            do { s = mulib_vmweave_run(&a,3); } while (s == MULIB_MIR_STEP_LIMIT);
            if (n < 21) {
                if (n) expected *= n;
                assert(s == MULIB_MIR_FINISHED && a.result.integer == expected);
            } else assert(s == MULIB_MIR_OVERFLOW && a.pc == 6);
            if (jit) {
                assert(mulib_mir_frame_init(&b,mulib_jit_flow_program(jit),&arg,1,NULL,NULL,100,NULL) == MULIB_MIR_OK);
                assert(mulib_jit_flow_run(jit,&a,100) == MULIB_MIR_INVALID);
                assert(mulib_jit_flow_run(jit,&b,0) == MULIB_MIR_STEP_LIMIT && !b.steps);
                do { t = mulib_jit_flow_run(jit,&b,2); } while (t == MULIB_MIR_STEP_LIMIT);
                assert(s == t && a.pc == b.pc && a.steps == b.steps);
                assert(a.result.integer == b.result.integer);
                assert(mulib_jit_flow_run(jit,&b,100) == s);
                mulib_mir_frame_free(&b);
            }
            assert(mulib_vmweave_run(&a,100) == s);
            mulib_mir_frame_free(&a);
        }
        assert(mulib_jit_flow_destroy(jit) == 0);
        mulib_vmweave_program_destroy(vm);
    }
}

typedef struct host_context {
    unsigned calls;
    int suspend, bad_bool;
    mulib_mir_frame_t *frame;
} host_context;
static mulib_mir_status_t host(void *user, size_t index,
                             const mulib_mir_value_t *args, size_t n,
                             mulib_mir_value_t *out)
{
    host_context *c = (host_context *)user;
    assert(index == 0 && n == 2);
    ++c->calls;
    if (c->frame) {
        assert(mulib_vmweave_run(c->frame,100) == MULIB_MIR_INVALID);
        mulib_mir_frame_free(c->frame);
        assert(c->frame->program);
    }
    out->integer = c->bad_bool ? 2 : args[0].integer + args[1].integer;
    return c->suspend ? MULIB_MIR_SUSPENDED : MULIB_MIR_OK;
}
static void run_hosts(void)
{
    mulib_mir_type_t types[] = {MULIB_MIR_I64,MULIB_MIR_I64,MULIB_MIR_I64};
    mulib_mir_flow_instruction_t code[] = {
        LIT(0,20), LIT(1,22), INS(CALL,2,0,2,0), INS(RETURN,N,2,N,N)
    };
    mulib_mir_signature_t signature = {ints,2,MULIB_MIR_I64};
    mulib_mir_flow_program_t p = {code,4,types,3,{NULL,0,MULIB_MIR_I64},&signature,1};
    mulib_vmweave_program_t *vm = NULL;
    mulib_jit_flow_t *jit = NULL;
    mulib_mir_frame_t f = {0};
    mulib_mir_value_t resume = {.integer=42};
    host_context ctx = {0,1,0,&f};
    assert(mulib_vmweave_compile(&p,100,1,&vm,NULL) == MULIB_MIR_OK);
    if (mulib_jit_supported()) assert(mulib_jit_flow_compile(&p,100,1,&jit,NULL) == 0);
    /* Both owners must outlive all source arrays and deep-copy signatures. */
    code[0].immediate.integer = 999; signature.parameter_count = 99; types[0] = MULIB_MIR_BOOL;
    assert(mulib_mir_frame_init(&f,mulib_vmweave_program_view(vm),NULL,0,host,&ctx,100,NULL) == MULIB_MIR_OK);
    assert(mulib_vmweave_run(&f,100) == MULIB_MIR_SUSPENDED && f.pc == 2 && f.steps == 3);
    assert(mulib_vmweave_run(&f,100) == MULIB_MIR_SUSPENDED && ctx.calls == 1);
    assert(mulib_mir_frame_resume(&f,resume) == MULIB_MIR_OK);
    assert(mulib_vmweave_run(&f,100) == MULIB_MIR_FINISHED && f.result.integer == 42 && ctx.calls == 1);
    assert(mulib_mir_frame_resume(&f,resume) == MULIB_MIR_INVALID);
    mulib_mir_frame_free(&f);
    if (jit) {
        assert(mulib_mir_frame_init(&f,mulib_jit_flow_program(jit),NULL,0,host,&ctx,100,NULL) == MULIB_MIR_OK);
        assert(mulib_jit_flow_run(jit,&f,100) == MULIB_MIR_SUSPENDED && ctx.calls == 2);
        assert(mulib_jit_flow_run(jit,&f,100) == MULIB_MIR_SUSPENDED && ctx.calls == 2);
        assert(mulib_mir_frame_resume(&f,resume) == MULIB_MIR_OK);
        assert(mulib_jit_flow_run(jit,&f,100) == MULIB_MIR_FINISHED && f.result.integer == 42);
        mulib_mir_frame_free(&f);
    }
    assert(mulib_jit_flow_destroy(jit) == 0);
    mulib_vmweave_program_destroy(vm);

    signature.parameter_count = 2; signature.result = MULIB_MIR_BOOL;
    types[0] = MULIB_MIR_I64; types[2] = MULIB_MIR_BOOL; p.signature.result = MULIB_MIR_BOOL;
    ctx.bad_bool = 1; ctx.suspend = 0;
    assert(mulib_mir_frame_init(&f,&p,NULL,0,host,&ctx,100,NULL) == MULIB_MIR_OK);
    assert(mulib_vmweave_run(&f,100) == MULIB_MIR_HOST_ERROR && f.pc == 2 && f.registers[2].integer == 0);
    mulib_mir_frame_free(&f);
    ctx.suspend = 1;
    assert(mulib_mir_frame_init(&f,&p,NULL,0,host,&ctx,100,NULL) == MULIB_MIR_OK);
    assert(mulib_vmweave_run(&f,100) == MULIB_MIR_SUSPENDED);
    assert(mulib_mir_frame_resume(&f,resume) == MULIB_MIR_BAD_TYPE && f.status == MULIB_MIR_SUSPENDED);
    resume.integer = 1;
    assert(mulib_mir_frame_resume(&f,resume) == MULIB_MIR_OK);
    assert(mulib_vmweave_run(&f,1) == MULIB_MIR_FINISHED && f.result.integer == 1);
    mulib_mir_frame_free(&f);
}

static mulib_mir_status_t evaluate(const mulib_mir_flow_program_t *p,
                                 int optimize, mulib_mir_value_t *result)
{
    mulib_vmweave_program_t *vm = NULL;
    mulib_mir_frame_t f = {0};
    mulib_mir_status_t s;
    assert(mulib_vmweave_compile(p,100,optimize,&vm,NULL) == MULIB_MIR_OK);
    assert(mulib_mir_frame_init(&f,mulib_vmweave_program_view(vm),NULL,0,NULL,NULL,100,NULL) == MULIB_MIR_OK);
    s = mulib_vmweave_run(&f,100);
    *result = f.result;
    mulib_mir_frame_free(&f); mulib_vmweave_program_destroy(vm);
    return s;
}
static void run_numbers(void)
{
    const int64_t numbers[] = {INT64_MIN, INT64_MIN+1,-2,-1,0,1,2,INT64_MAX};
    const double floats[] = {-INFINITY,-0x1p63,-3.5,-0.0,0.0,1.0,0x1p63,INFINITY,NAN};
    mulib_mir_type_t types[] = {MULIB_MIR_I64,MULIB_MIR_I64,MULIB_MIR_I64};
    mulib_mir_flow_instruction_t code[] = {LIT(0,0),LIT(1,0),INS(ADD,2,0,1,N),INS(RETURN,N,2,N,N)};
    mulib_mir_flow_program_t p = {code,4,types,3,{NULL,0,MULIB_MIR_I64},NULL,0};
    int op;
    size_t a,b;
    for (op = MULIB_MIR_FLOW_ADD; op <= MULIB_MIR_FLOW_SHR; ++op) {
        if (op == MULIB_MIR_FLOW_NOT) continue;
        for (a=0;a<sizeof(numbers)/sizeof(numbers[0]);++a) for(b=0;b<sizeof(numbers)/sizeof(numbers[0]);++b) {
            mulib_mir_value_t x,y;
            mulib_mir_status_t s,t;
            code[0].immediate.integer=numbers[a]; code[1].immediate.integer=numbers[b];
            code[2].opcode=(mulib_mir_flow_opcode_t)op; code[2].b=op==MULIB_MIR_FLOW_NEG?N:1;
            types[2]=p.signature.result=op>=MULIB_MIR_FLOW_EQ&&op<=MULIB_MIR_FLOW_LE?MULIB_MIR_BOOL:MULIB_MIR_I64;
            s=evaluate(&p,0,&x); t=evaluate(&p,1,&y);
            assert(s==t && (s!=MULIB_MIR_FINISHED || x.integer==y.integer));
        }
    }
    types[0]=types[1]=MULIB_MIR_F64; types[2]=p.signature.result=MULIB_MIR_I64;
    code[2].opcode=MULIB_MIR_FLOW_F64_I64; code[2].b=N;
    for(a=0;a<sizeof(floats)/sizeof(floats[0]);++a) {
        mulib_mir_value_t x,y;
        mulib_mir_status_t s,t;
        code[0].immediate.number=floats[a]; code[1].immediate.number=0;
        s=evaluate(&p,0,&x); t=evaluate(&p,1,&y);
        assert(s==t && (s!=MULIB_MIR_FINISHED || x.integer==y.integer));
        assert(s==(floats[a]>=-0x1p63&&floats[a]<0x1p63?MULIB_MIR_FINISHED:MULIB_MIR_OVERFLOW));
    }
    types[2]=p.signature.result=MULIB_MIR_F64;
    for(op=MULIB_MIR_FLOW_ADD;op<=MULIB_MIR_FLOW_NEG;++op)
        for(a=0;a<sizeof(floats)/sizeof(floats[0]);++a) for(b=0;b<sizeof(floats)/sizeof(floats[0]);++b) {
            mulib_mir_value_t x,y;
            code[0].immediate.number=floats[a]; code[1].immediate.number=floats[b];
            code[2].opcode=(mulib_mir_flow_opcode_t)op; code[2].b=op==MULIB_MIR_FLOW_NEG?N:1;
            assert(evaluate(&p,0,&x)==MULIB_MIR_FINISHED);
            assert(evaluate(&p,1,&y)==MULIB_MIR_FINISHED);
            assert((isnan(x.number)&&isnan(y.number)) || (x.number==y.number && !!signbit(x.number)==!!signbit(y.number)));
        }
}

static void malformed(void)
{
    mulib_mir_flow_program_t p = factorial;
    mulib_mir_flow_instruction_t code[9];
    mulib_mir_diagnostic_t d;
    mulib_vmweave_program_t *vm = NULL;
    mulib_mir_frame_t f = {0};
    mulib_mir_value_t arg = {.integer=5};
    memcpy(code,factorial_code,sizeof(code)); p.code=code;
    assert(mulib_mir_flow_validate(NULL,100,NULL)==MULIB_MIR_INVALID);
    assert(mulib_mir_flow_validate(&p,8,NULL)==MULIB_MIR_RESOURCE_LIMIT);
    code[4].target=9;
    assert(mulib_vmweave_compile(&p,100,0,&vm,&d)==MULIB_MIR_BAD_REFERENCE && !vm && d.record==4);
    code[4].target=6; code[4].a=0;
    assert(mulib_mir_flow_validate(&p,100,&d)==MULIB_MIR_BAD_TYPE && d.record==4);
    code[4].a=3; code[6].destination=3;
    assert(mulib_mir_flow_validate(&p,100,NULL)==MULIB_MIR_BAD_TYPE);
    code[6].destination=1; code[6].opcode=(mulib_mir_flow_opcode_t)999;
    assert(mulib_mir_flow_validate(&p,100,NULL)==MULIB_MIR_BAD_OPCODE);
    code[6]=factorial_code[6]; code[8]=factorial_code[3];
    assert(mulib_mir_flow_validate(&p,100,NULL)==MULIB_MIR_BAD_RETURN);
    code[8]=factorial_code[8]; code[0].a=1;
    assert(mulib_mir_flow_validate(&p,100,NULL)==MULIB_MIR_BAD_REFERENCE);
    code[0]=factorial_code[0]; code[0].immediate.word=1;
    assert(mulib_mir_flow_validate(&p,100,NULL)==MULIB_MIR_INVALID);
    code[0]=factorial_code[0];
    assert(mulib_mir_frame_init(&f,&p,NULL,0,NULL,NULL,100,NULL)==MULIB_MIR_INVALID && !f.program);
    assert(mulib_mir_frame_init(&f,&p,&arg,1,NULL,NULL,100,NULL)==MULIB_MIR_OK);
    assert(mulib_mir_frame_init(&f,&p,&arg,1,NULL,NULL,100,NULL)==MULIB_MIR_INVALID);
    mulib_mir_frame_free(&f); mulib_mir_frame_free(&f); mulib_mir_frame_free(NULL);
}
static void identities(void)
{
    static const mulib_mir_flow_opcode_t ops[] = {
        MULIB_MIR_FLOW_ADD,MULIB_MIR_FLOW_SUB,MULIB_MIR_FLOW_MUL,
        MULIB_MIR_FLOW_WRAP_ADD,MULIB_MIR_FLOW_WRAP_SUB,MULIB_MIR_FLOW_WRAP_MUL,
        MULIB_MIR_FLOW_OR,MULIB_MIR_FLOW_XOR,MULIB_MIR_FLOW_SHL,MULIB_MIR_FLOW_SHR
    };
    mulib_mir_type_t types[]={MULIB_MIR_I64,MULIB_MIR_I64};
    mulib_mir_flow_instruction_t code[]={INS(PARAM,0,0,N,N),LIT(1,0),INS(ADD,0,0,1,N),INS(RETURN,N,0,N,N)};
    mulib_mir_flow_program_t p={code,4,types,2,{types,1,MULIB_MIR_I64},NULL,0};
    size_t i;
    for(i=0;i<sizeof(ops)/sizeof(ops[0]);++i) {
        mulib_vmweave_program_t *vm=NULL;
        mulib_mir_frame_t f={0};
        mulib_mir_value_t arg={.integer=INT64_MIN};
        code[2].opcode=ops[i];
        code[1].immediate.integer=ops[i]==MULIB_MIR_FLOW_MUL || ops[i]==MULIB_MIR_FLOW_WRAP_MUL;
        assert(mulib_vmweave_compile(&p,100,1,&vm,NULL)==MULIB_MIR_OK);
        assert(mulib_vmweave_program_view(vm)->code[2].opcode==MULIB_MIR_FLOW_COPY);
        assert(mulib_mir_frame_init(&f,mulib_vmweave_program_view(vm),&arg,1,NULL,NULL,100,NULL)==MULIB_MIR_OK);
        assert(mulib_vmweave_run(&f,100)==MULIB_MIR_FINISHED && f.result.integer==INT64_MIN);
        mulib_mir_frame_free(&f); mulib_vmweave_program_destroy(vm);
    }
    {
        mulib_vmweave_program_t *vm=NULL;
        mulib_mir_frame_t f={0};
        mulib_mir_value_t arg={.number=-0.0};
        types[0]=types[1]=p.signature.result=MULIB_MIR_F64;
        code[1].immediate.number=0.0; code[2].opcode=MULIB_MIR_FLOW_ADD;
        assert(mulib_vmweave_compile(&p,100,1,&vm,NULL)==MULIB_MIR_OK);
        assert(mulib_vmweave_program_view(vm)->code[2].opcode==MULIB_MIR_FLOW_ADD);
        assert(mulib_mir_frame_init(&f,mulib_vmweave_program_view(vm),&arg,1,NULL,NULL,100,NULL)==MULIB_MIR_OK);
        assert(mulib_vmweave_run(&f,100)==MULIB_MIR_FINISHED && !signbit(f.result.number));
        mulib_mir_frame_free(&f); mulib_vmweave_program_destroy(vm);
    }
}
static int write_file(void *user, const char *bytes, size_t n)
{
    return fwrite(bytes,1,n,(FILE *)user)==n;
}
static int fail_write(void *user, const char *bytes, size_t n)
{
    (void)user; (void)bytes; (void)n; return 0;
}
static void emit(void)
{
    static const uint64_t floating_bits[]={UINT64_C(0x8000000000000000),UINT64_C(0xfff8000000000042),UINT64_C(0x0000000000000001)};
    size_t k;
    puts("#define MULIB_IMPL_MIR\n#include \"mulib-mir.h\"\n#include <assert.h>");
    assert(mulib_mir_flow_emit_c(&factorial,"mulib_generated_factorial",100,write_file,stdout,NULL)==MULIB_MIR_OK);
    for(k=0;k<sizeof(floating_bits)/sizeof(floating_bits[0]);++k) {
        mulib_mir_type_t type=MULIB_MIR_F64;
        mulib_mir_flow_instruction_t code[]={FLIT(0,0.0),INS(RETURN,N,0,N,N)};
        mulib_mir_flow_program_t p={code,2,&type,1,{NULL,0,MULIB_MIR_F64},NULL,0};
        char name[64];
        memcpy(&code[0].immediate.number,&floating_bits[k],sizeof(uint64_t));
        snprintf(name,sizeof(name),"mulib_generated_floating%zu",k);
        assert(mulib_mir_flow_emit_c(&p,name,100,write_file,stdout,NULL)==MULIB_MIR_OK);
    }
    puts("int main(void) { mulib_mir_frame_t f={0}; mulib_mir_value_t a={.integer=10};");
    puts("assert(mulib_mir_frame_init(&f,mulib_generated_factorial_program(),&a,1,NULL,NULL,100,NULL)==MULIB_MIR_OK);");
    puts("f.remaining=3; assert(mulib_generated_factorial(&f)==MULIB_MIR_STEP_LIMIT); f.remaining=100;");
    puts("assert(mulib_generated_factorial(&f)==MULIB_MIR_FINISHED && f.result.integer==3628800);");
    puts("mulib_mir_frame_free(&f);");
    for(k=0;k<sizeof(floating_bits)/sizeof(floating_bits[0]);++k) {
        printf("assert(mulib_mir_frame_init(&f,mulib_generated_floating%zu_program(),NULL,0,NULL,NULL,100,NULL)==MULIB_MIR_OK); f.remaining=10;\n",k);
        printf("assert(mulib_generated_floating%zu(&f)==MULIB_MIR_FINISHED && f.result.word==UINT64_C(%" PRIu64 ")); mulib_mir_frame_free(&f);\n",k,floating_bits[k]);
    }
    puts("return 0; }");
}
int main(int argc, char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--emit")) { emit(); return 0; }
    run_factorial(); run_hosts(); run_numbers(); malformed(); identities();
    assert(mulib_mir_flow_emit_c(&factorial,"bad",100,fail_write,NULL,NULL)==MULIB_MIR_INVALID);
    assert(mulib_mir_flow_emit_c(&factorial,"mulib_generated_test",100,fail_write,NULL,NULL)==MULIB_MIR_IO_ERROR);
    puts("MIR control-flow, VM, and native JIT tests passed");
    return 0;
}
