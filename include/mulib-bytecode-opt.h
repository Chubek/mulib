/* MuLIB compiler tools: Jump-free algebraic constant folding with checked signed integer semantics.
 * ISO C11 single-header module. Define MULIB_IMPL_BYTECODE_OPT in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_BYTECODE_OPT_H
#define MULIB_BYTECODE_OPT_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-bytecode.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-bytecode.h"
/* Edits owned buffer in place. Returns 0 when jumps or malformed opcodes exist. */
int mulib_bytecode_optimize(mulib_bc_instruction_t *code,size_t *count,size_t *folded);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BYTECODE_OPT_H */
#if defined(MULIB_IMPL_BYTECODE_OPT) && !defined(MULIB_BYTECODE_OPT_IMPLEMENTED)
#define MULIB_BYTECODE_OPT_IMPLEMENTED
#include "detail/mulib-compiler-integer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_bytecode_optimize(mulib_bc_instruction_t *p,size_t *count,size_t *folded){if(!p||!count)return 0;
size_t n=*count;
 if(n>1048576)return 0;
 for(size_t i=0;i<n;i++)if((unsigned)p[i].op>MULIB_BC_HALT||p[i].op==MULIB_BC_JMP||p[i].op==MULIB_BC_JZ)return 0;
 size_t total=0;
int changed;
do{changed=0;
for(size_t i=0;i+2<n;i++){
 if(p[i].op!=MULIB_BC_PUSH||p[i+1].op!=MULIB_BC_PUSH)continue;
int64_t a=p[i].operand,b=p[i+1].operand,v=0;
int good=0;
 switch(p[i+2].op){case MULIB_BC_ADD:good=mulib_compiler_add_i64_(a,b,&v);
break;
case MULIB_BC_SUB:good=mulib_compiler_sub_i64_(a,b,&v);
break;
case MULIB_BC_MUL:good=mulib_compiler_mul_i64_(a,b,&v);
break;
case MULIB_BC_DIV:if(b&&!(a==INT64_MIN&&b==-1)){v=a/b;
good=1;
}break;
case MULIB_BC_EQ:v=(a==b);
good=1;
break;
default:break;
}
 if(good){p[i].operand=v;
memmove(p+i+1,p+i+3,(n-i-3)*sizeof(*p));
n-=2;
total++;
changed=1;
break;
}
 }}while(changed);
 *count=n;
if(folded)*folded=total;
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
