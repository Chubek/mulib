#define MULIB_IMPL_BYTECODE
#define MULIB_IMPL_BYTECODE_BUILDER
#define MULIB_IMPL_BYTECODE_DISASM
#define MULIB_IMPL_BYTECODE_EVAL
#define MULIB_IMPL_BYTECODE_OPT
#define MULIB_IMPL_BYTECODE_SERIALIZE
#define MULIB_IMPL_BYTECODE_VERIFY
#include "mulib-bytecode-builder.h"
#include "mulib-bytecode-disasm.h"
#include "mulib-bytecode-eval.h"
#include "mulib-bytecode-opt.h"
#include "mulib-bytecode-serialize.h"
#include <assert.h>

static void arithmetic(void) {
    const int64_t values[] = {INT64_MIN, INT64_MIN+1, -3, -1, 0, 1, 3, INT64_MAX};
    for(size_t a=0;a<8;a++) for(size_t b=0;b<8;b++)
    for(int op=MULIB_BC_ADD;op<=MULIB_BC_EQ;op++) {
        mulib_bc_instruction_t p[]={{MULIB_BC_PUSH,values[a]}, {MULIB_BC_PUSH,values[b]},
                                  {(mulib_bc_opcode_t)op,0},{MULIB_BC_HALT,0}};
        mulib_bc_result_t before,after;
        size_t n=4,folded=99;
        mulib_bc_status_t status=mulib_bytecode_run(p,n,0,2,10,&before);
        assert(mulib_bytecode_optimize(p,&n,&folded));
        assert(mulib_bytecode_run(p,n,0,2,10,&after)==status);
        if(status==MULIB_BC_OK) {assert(after.value==before.value);assert(n==2&&folded==1);}
        else {assert(status==MULIB_BC_ARITHMETIC);assert(n==4&&folded==0);}
    }
    mulib_bc_instruction_t overflow[]={{MULIB_BC_PUSH,INT64_MAX},{MULIB_BC_PUSH,1},
                                     {MULIB_BC_ADD,0},{MULIB_BC_HALT,0}};
    mulib_bc_result_t r;
    assert(mulib_bytecode_run(overflow,4,0,2,10,&r)==MULIB_BC_ARITHMETIC&&r.pc==2);
    overflow[0].operand=INT64_MIN;overflow[1].operand=-1;overflow[2].op=MULIB_BC_DIV;
    assert(mulib_bytecode_run(overflow,4,0,2,10,&r)==MULIB_BC_ARITHMETIC);
}
int main(void) {
    mulib_bytecode_builder_t b={0};size_t index=88,peak,bad;
    assert(!mulib_bytecode_builder_add(&b,(mulib_bc_opcode_t)-1,0,&index)&&index==88);
    assert(mulib_bytecode_builder_add(&b,MULIB_BC_PUSH,6,&index)&&index==0);
    assert(mulib_bytecode_builder_add(&b,MULIB_BC_PUSH,7,NULL));
    assert(mulib_bytecode_builder_add(&b,MULIB_BC_MUL,0,NULL));
    assert(mulib_bytecode_builder_add(&b,MULIB_BC_HALT,0,NULL));
    assert(!mulib_bytecode_builder_patch(&b,4,0));
    assert(mulib_bytecode_builder_patch(&b,1,7));
    assert(mulib_bytecode_verify(b.code,b.count,0,&peak,&bad)&&peak==2&&bad==SIZE_MAX);
    mulib_bc_result_t r;
    assert(mulib_bytecode_eval(b.code,b.count,0,4,&r)&&r.value==42&&r.steps==4);
    assert(!mulib_bytecode_eval(b.code,b.count,0,3,&r)&&r.status==MULIB_BC_LIMIT);
    assert(mulib_bytecode_run(b.code,b.count,0,1,4,&r)==MULIB_BC_STACK);
    char text[128];
    assert(mulib_bytecode_disassemble(b.code,b.count,text,sizeof text));
    assert(!strcmp(text,"0: PUSH 6\n1: PUSH 7\n2: MUL 0\n3: HALT 0\n"));
    assert(!mulib_bytecode_disassemble(b.code,b.count,text,1)&&text[0]==0);
    b.code[0].operand=INT64_MIN;b.code[1].operand=INT64_MAX;
    unsigned char *bytes=NULL;size_t length=0,count=0;
    assert(mulib_bytecode_encode(b.code,b.count,&bytes,&length)&&length==48);
    assert(!memcmp(bytes,"MBC1\4\0\0\0",8));
    mulib_bc_instruction_t *decoded=NULL;
    for(size_t i=0;i<length;i++)assert(!mulib_bytecode_decode(bytes,i,&decoded,&count)&&!decoded);
    for(size_t i=0;i<length;i++) {
        bytes[i]^=1;
        assert(!mulib_bytecode_decode(bytes,length,&decoded,&count)&&!decoded);
        bytes[i]^=1;
    }
    assert(mulib_bytecode_decode(bytes,length,&decoded,&count)&&count==4);
    assert(decoded[0].operand==INT64_MIN&&decoded[1].operand==INT64_MAX);
    free(decoded);free(bytes);mulib_bytecode_builder_free(&b);
    assert(!b.code&&!b.count);mulib_bytecode_builder_free(&b);
    mulib_bc_instruction_t loop[]={{MULIB_BC_JMP,0}};
    assert(mulib_bytecode_run(loop,1,0,0,9,&r)==MULIB_BC_LIMIT&&r.steps==9);
    assert(!mulib_bytecode_verify(loop,1,0,&peak,&bad)&&bad==1);
    mulib_bc_instruction_t join[]={{MULIB_BC_PUSH,0},{MULIB_BC_JZ,4},
        {MULIB_BC_PUSH,1},{MULIB_BC_JMP,4},{MULIB_BC_PUSH,2},{MULIB_BC_HALT,0}};
    assert(!mulib_bytecode_verify(join,6,0,&peak,&bad));
    mulib_bc_instruction_t unreachable[]={{MULIB_BC_PUSH,1},{MULIB_BC_HALT,0},{(mulib_bc_opcode_t)-1,0}};
    assert(!mulib_bytecode_eval(unreachable,3,0,10,&r)&&r.status==MULIB_BC_INVALID&&r.pc==2&&r.steps==0);
    mulib_bc_instruction_t badjump[]={{MULIB_BC_PUSH,1},{MULIB_BC_JZ,99},{MULIB_BC_HALT,0}};
    assert(mulib_bytecode_run(badjump,3,0,2,10,&r)==MULIB_BC_INVALID);
    mulib_bc_instruction_t locals[]={{MULIB_BC_PUSH,42},{MULIB_BC_STORE,0},{MULIB_BC_LOAD,0},{MULIB_BC_HALT,0}};
    assert(mulib_bytecode_eval(locals,4,1,4,&r)&&r.value==42);
    assert(!mulib_bytecode_eval(locals,4,0,4,&r));
    arithmetic();
    return 0;
}
