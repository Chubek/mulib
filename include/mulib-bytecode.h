/* MuLIB compiler tools: Versioned checked signed-64 stack bytecode with interpreter and instruction budgets.
 * ISO C11 single-header module. Define MULIB_IMPL_BYTECODE in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_BYTECODE_H
#define MULIB_BYTECODE_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum {MULIB_BC_PUSH,MULIB_BC_LOAD,MULIB_BC_STORE,MULIB_BC_ADD,MULIB_BC_SUB,MULIB_BC_MUL,MULIB_BC_DIV,MULIB_BC_EQ,MULIB_BC_JMP,MULIB_BC_JZ,MULIB_BC_HALT} mulib_bc_opcode_t;
typedef struct {mulib_bc_opcode_t op;
int64_t operand;
} mulib_bc_instruction_t;
typedef enum {MULIB_BC_OK,MULIB_BC_INVALID,MULIB_BC_STACK,MULIB_BC_ARITHMETIC,MULIB_BC_LIMIT,MULIB_BC_OOM} mulib_bc_status_t;
typedef struct {mulib_bc_status_t status;
size_t pc,steps;
int64_t value;
} mulib_bc_result_t;
/* Code is immutable. Only HALT succeeds; budget exhaustion returns LIMIT. */
mulib_bc_status_t mulib_bytecode_run(const mulib_bc_instruction_t *code,size_t n,size_t local_count,size_t stack_limit,size_t step_limit,mulib_bc_result_t *result);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BYTECODE_H */
#if defined(MULIB_IMPL_BYTECODE) && !defined(MULIB_BYTECODE_IMPLEMENTED)
#define MULIB_BYTECODE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_bc_add_(int64_t a,int64_t b,int64_t *out){if((b>0&&a>INT64_MAX-b)||(b<0&&a<INT64_MIN-b))return 0;
*out=a+b;
return 1;
}
static int mulib_bc_sub_(int64_t a,int64_t b,int64_t *out){if((b<0&&a>INT64_MAX+b)||(b>0&&a<INT64_MIN+b))return 0;
*out=a-b;
return 1;
}
static int mulib_bc_mul_(int64_t a,int64_t b,int64_t *out){if(!a||!b){*out=0;
return 1;
}if((a==INT64_MIN&&b==-1)||(b==INT64_MIN&&a==-1))return 0;
 if(a>0){if((b>0&&a>INT64_MAX/b)||(b<0&&b<INT64_MIN/a))return 0;
}
 else {if((b>0&&a<INT64_MIN/b)||(b<0&&a<INT64_MAX/b))return 0;
}*out=a*b;
return 1;
}
mulib_bc_status_t mulib_bytecode_run(const mulib_bc_instruction_t *code,size_t n,size_t locals_n,size_t limit,size_t budget,mulib_bc_result_t *res){
 if(!res)return MULIB_BC_INVALID;
mulib_bc_result_t r={MULIB_BC_INVALID,0,0,0};
 if(!code||!n||n>1048576||locals_n>1048576||limit>1048576)goto done;
 int64_t *locals=(int64_t*)calloc(locals_n?locals_n:1,sizeof(int64_t));
int64_t *stack=(int64_t*)malloc((limit?limit:1)*sizeof(int64_t));
 if(!locals||!stack){free(locals);
free(stack);
r.status=MULIB_BC_OOM;
goto done;
}
 size_t pc=0,sp=0;
while(pc<n){r.pc=pc;
if(r.steps>=budget){r.status=MULIB_BC_LIMIT;
break;
}r.steps++;
 mulib_bc_instruction_t ins=code[pc];
int64_t a=0,b=0,value=0;
size_t target=0;
 switch(ins.op){
 case MULIB_BC_PUSH:if(sp>=limit){r.status=MULIB_BC_STACK;
goto cleanup;
}stack[sp++]=ins.operand;
pc++;
break;
 case MULIB_BC_LOAD:if(ins.operand<0||(uint64_t)ins.operand>=locals_n){r.status=MULIB_BC_INVALID;
goto cleanup;
}if(sp>=limit){r.status=MULIB_BC_STACK;
goto cleanup;
}stack[sp++]=locals[(size_t)ins.operand];
pc++;
break;
 case MULIB_BC_STORE:if(ins.operand<0||(uint64_t)ins.operand>=locals_n){r.status=MULIB_BC_INVALID;
goto cleanup;
}if(!sp){r.status=MULIB_BC_STACK;
goto cleanup;
}locals[(size_t)ins.operand]=stack[--sp];
pc++;
break;
 case MULIB_BC_ADD:case MULIB_BC_SUB:case MULIB_BC_MUL:case MULIB_BC_DIV:case MULIB_BC_EQ:
 if(sp<2){r.status=MULIB_BC_STACK;
goto cleanup;
}b=stack[--sp];
a=stack[sp-1];
 if(ins.op==MULIB_BC_ADD? !mulib_bc_add_(a,b,&value):ins.op==MULIB_BC_SUB? !mulib_bc_sub_(a,b,&value):ins.op==MULIB_BC_MUL? !mulib_bc_mul_(a,b,&value):0){r.status=MULIB_BC_ARITHMETIC;
goto cleanup;
}
 if(ins.op==MULIB_BC_DIV){if(!b||(a==INT64_MIN&&b==-1)){r.status=MULIB_BC_ARITHMETIC;
goto cleanup;
}value=a/b;
}
 if(ins.op==MULIB_BC_EQ)value=a==b;
 stack[sp-1]=value;
pc++;
break;
  case MULIB_BC_JZ:if(ins.operand<0||(uint64_t)ins.operand>=n){r.status=MULIB_BC_INVALID;
goto cleanup;
}if(!sp){r.status=MULIB_BC_STACK;
goto cleanup;
}value=stack[--sp];
if(value){pc++;
break;
}
 /* fall through */
 case MULIB_BC_JMP:if(ins.operand<0||(uint64_t)ins.operand>=n){r.status=MULIB_BC_INVALID;
goto cleanup;
}target=(size_t)ins.operand;
pc=target;
break;
 case MULIB_BC_HALT:if(!sp){r.status=MULIB_BC_STACK;
goto cleanup;
}r.value=stack[sp-1];
r.status=MULIB_BC_OK;
goto cleanup;
 default:r.status=MULIB_BC_INVALID;
goto cleanup;
}
 }
 if(pc>=n)r.status=MULIB_BC_INVALID;
 cleanup:free(locals);
free(stack);
 done:*res=r;
return r.status;
}
#ifdef __cplusplus
}
#endif
#endif
