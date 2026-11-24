/** @file mulib-kernel-validate.h
 * Validate SSA dominance/types and interpret scalar expressions.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No execution budgets beyond bounded instruction count; callers enforce finite input if needed.
 */
#ifndef MULIB_KERNEL_VALIDATE_H
#define MULIB_KERNEL_VALIDATE_H
#include "mulib-kernel-ir.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
static inline bool mulib_kernel_validate(mulib_kernel_ir ir){if(!ir.code||!ir.count||ir.count>UINT32_MAX||ir.count>SIZE_MAX/sizeof *ir.code||ir.result>=ir.count)return false;for(size_t i=0;i<ir.count;i++){mulib_kernel_inst x=ir.code[i];unsigned ar=mulib_kernel_ir_arity(x.op);if(ar==UINT_MAX||(x.type!=MULIB_KERNEL_F32&&x.type!=MULIB_KERNEL_BOOL)||(ar>0&&x.a>=i)||(ar>1&&x.b>=i)||(ar>2&&x.c>=i))return false;if(x.op==MULIB_KERNEL_PARAM){if(x.a>=ir.param_count||x.type!=MULIB_KERNEL_F32)return false;continue;}if(x.op==MULIB_KERNEL_CONST){if(!isfinite(x.literal)||(x.type==MULIB_KERNEL_BOOL&&x.literal!=0&&x.literal!=1))return false;continue;}if(x.op==MULIB_KERNEL_SELECT){if(ir.code[x.a].type!=MULIB_KERNEL_BOOL||ir.code[x.b].type!=x.type||ir.code[x.c].type!=x.type)return false;continue;}if(ir.code[x.a].type!=MULIB_KERNEL_F32||(ar==2&&ir.code[x.b].type!=MULIB_KERNEL_F32)||x.type!=(x.op==MULIB_KERNEL_LT?MULIB_KERNEL_BOOL:MULIB_KERNEL_F32))return false;}return true;}
static inline float mulib_kernel_validate_eval_inst(mulib_kernel_inst x,float a,float b,float c){switch(x.op){case MULIB_KERNEL_CONST:return x.literal;case MULIB_KERNEL_ADD:return a+b;case MULIB_KERNEL_SUB:return a-b;case MULIB_KERNEL_MUL:return a*b;case MULIB_KERNEL_DIV:return a/b;case MULIB_KERNEL_MIN:return fminf(a,b);case MULIB_KERNEL_MAX:return fmaxf(a,b);case MULIB_KERNEL_NEG:return -a;case MULIB_KERNEL_LT:return a<b?1:0;case MULIB_KERNEL_SELECT:return a!=0?b:c;default:return NAN;}}
/* params_count and cap are float elements. Scratch must be disjoint from
 * params and code. out may alias scratch or params, but not code. On validation
 * failure nothing is written; IEEE exceptional arithmetic is not a failure. */
static inline bool mulib_kernel_eval(mulib_kernel_ir ir,const float *params,size_t params_count,float *scratch,size_t cap,float *out){
 size_t pb,sb,cb;
 if(!mulib_kernel_validate(ir)||params_count<ir.param_count||(!params&&ir.param_count)||!scratch||cap<ir.count||!out||
    !mulib_media_size_mul(ir.param_count,sizeof *params,&pb)||!mulib_media_size_mul(ir.count,sizeof *scratch,&sb)||
    !mulib_media_size_mul(ir.count,sizeof *ir.code,&cb)||
    !mulib_kernel_ir_disjoint(scratch,sb,params,pb)||!mulib_kernel_ir_disjoint(scratch,sb,ir.code,cb)||
    !mulib_kernel_ir_disjoint(out,sizeof *out,ir.code,cb))return false;
 for(size_t i=0;i<ir.count;i++){mulib_kernel_inst x=ir.code[i];unsigned a=mulib_kernel_ir_arity(x.op);scratch[i]=x.op==MULIB_KERNEL_PARAM?params[x.a]:mulib_kernel_validate_eval_inst(x,a?scratch[x.a]:0,a>1?scratch[x.b]:0,a>2?scratch[x.c]:0);}
 *out=scratch[ir.result];return true;
}
#ifdef __cplusplus
}
#endif
#endif
