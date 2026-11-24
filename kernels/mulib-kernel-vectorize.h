/** @file mulib-kernel-vectorize.h
 * Portable lane-wise batched SSA execution.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Scalar loop, not actual SIMD code generation.
 */
#ifndef MULIB_KERNEL_VECTORIZE_H
#define MULIB_KERNEL_VECTORIZE_H
#include "mulib-kernel-ir.h"
#include "mulib-kernel-validate.h"
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
/* Parameter-major input p[param*lanes+lane], scratch[ir.count], out[lanes]. */
/* p_cap, cap, out_cap count float elements. All three buffers and code must
 * be disjoint. Failure leaves outputs unchanged. Exceptional f32 values use
 * the same semantics as mulib_kernel_eval, not GPU/SIMD execution. */
static inline bool mulib_kernel_vectorize_eval(mulib_kernel_ir ir,const float *p,size_t p_cap,size_t lanes,float *scratch,size_t cap,float *out,size_t out_cap){
 size_t in_count,pb,sb,ob,cb;
 if(!mulib_kernel_validate(ir)||!lanes||!mulib_media_size_mul(ir.param_count,lanes,&in_count)||p_cap<in_count||out_cap<lanes||
    (!p&&in_count)||!scratch||cap<ir.count||!out||
    !mulib_media_size_mul(in_count,sizeof *p,&pb)||!mulib_media_size_mul(ir.count,sizeof *scratch,&sb)||
    !mulib_media_size_mul(lanes,sizeof *out,&ob)||!mulib_media_size_mul(ir.count,sizeof *ir.code,&cb)||
    !mulib_kernel_ir_disjoint(p,pb,scratch,sb)||!mulib_kernel_ir_disjoint(p,pb,out,ob)||
    !mulib_kernel_ir_disjoint(out,ob,scratch,sb)||!mulib_kernel_ir_disjoint(scratch,sb,ir.code,cb)||
    !mulib_kernel_ir_disjoint(out,ob,ir.code,cb))return false;
 for(size_t lane=0;lane<lanes;lane++){for(size_t i=0;i<ir.count;i++){mulib_kernel_inst x=ir.code[i];unsigned a=mulib_kernel_ir_arity(x.op);scratch[i]=x.op==MULIB_KERNEL_PARAM?p[(size_t)x.a*lanes+lane]:mulib_kernel_validate_eval_inst(x,a?scratch[x.a]:0,a>1?scratch[x.b]:0,a>2?scratch[x.c]:0);}out[lane]=scratch[ir.result];}return true;
}
#ifdef __cplusplus
}
#endif
#endif
