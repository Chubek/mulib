/* MuLIB compiler tools: Control-flow-aware stack-height verifier with branch-join checks.
 * ISO C11 single-header module. Define MULIB_IMPL_BYTECODE_VERIFY in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_BYTECODE_VERIFY_H
#define MULIB_BYTECODE_VERIFY_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-bytecode.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-bytecode.h"
/* Validates instruction/stack/locals bounds; returns max stack height. */
int mulib_bytecode_verify(const mulib_bc_instruction_t *code,size_t count,size_t local_count,size_t *max_stack,size_t *bad_pc);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BYTECODE_VERIFY_H */
#if defined(MULIB_IMPL_BYTECODE_VERIFY) && !defined(MULIB_BYTECODE_VERIFY_IMPLEMENTED)
#define MULIB_BYTECODE_VERIFY_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_bytecode_verify(const mulib_bc_instruction_t *code,size_t n,size_t locals,size_t *maximum,size_t *bad){
 if(bad)*bad=0;
 if(!code||!n||n>1048576||locals>1048576||!maximum)return 0;
 /* Check even unreachable instructions; stack analysis remains reachability based. */
 for(size_t i=0;i<n;i++){
 mulib_bc_instruction_t ins=code[i];
 if((unsigned)ins.op>MULIB_BC_HALT ||
 ((ins.op==MULIB_BC_LOAD||ins.op==MULIB_BC_STORE)&&
 (ins.operand<0||(uint64_t)ins.operand>=locals)) ||
 ((ins.op==MULIB_BC_JMP||ins.op==MULIB_BC_JZ)&&
 (ins.operand<0||(uint64_t)ins.operand>=n))){if(bad)*bad=i;return 0;}
 }
size_t *depth=(size_t*)malloc(n*sizeof(size_t)),*queue=(size_t*)malloc(n*sizeof(size_t));
if(!depth||!queue){free(depth);
free(queue);
return 0;
}
 for(size_t i=0;i<n;i++)depth[i]=SIZE_MAX;
size_t head=0,tail=1,peak=0;
int has_halt=0;
depth[0]=0;
queue[0]=0;
 while(head<tail){size_t pc=queue[head++],sp=depth[pc],need=0,push=0,targets[2],nt=0;
mulib_bc_instruction_t ins=code[pc];
 switch(ins.op){case MULIB_BC_PUSH:push=1;
break;
case MULIB_BC_LOAD:push=1;
/* fallthrough */
 case MULIB_BC_STORE:if(ins.operand<0||(uint64_t)ins.operand>=locals)goto invalid;
if(ins.op==MULIB_BC_STORE){need=1;
push=0;
}break;
 case MULIB_BC_ADD:case MULIB_BC_SUB:case MULIB_BC_MUL:case MULIB_BC_DIV:case MULIB_BC_EQ:need=2;
push=1;
break;
 case MULIB_BC_JZ:need=1;
/* fallthrough */
 case MULIB_BC_JMP:if(ins.operand<0||(uint64_t)ins.operand>=n)goto invalid;
targets[nt++]=(size_t)ins.operand;
break;
 case MULIB_BC_HALT:need=1;
has_halt=1;
break;
default:goto invalid;
}
 if(sp<need||sp-need>SIZE_MAX-push)goto invalid;
size_t after=sp-need+push;
if(after>peak)peak=after;
 if(ins.op==MULIB_BC_HALT)continue;
 if(ins.op!=MULIB_BC_JMP){if(pc+1>=n)goto invalid;
targets[nt++]=pc+1;
}
 for(size_t i=0;i<nt;i++){size_t v=targets[i];
if(depth[v]==SIZE_MAX){depth[v]=after;
queue[tail++]=v;
}else if(depth[v]!=after)goto invalid;
}
 continue;
 invalid:if(bad)*bad=pc;
free(depth);
free(queue);
return 0;
 }
 free(depth);
free(queue);
if(!has_halt){if(bad)*bad=n;
return 0;
}*maximum=peak;
if(bad)*bad=SIZE_MAX;
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
