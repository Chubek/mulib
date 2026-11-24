/** @file mulib-kernel-uniformity.h
 * Conservative SSA uniformity propagation from parameter annotations.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No divergent control-flow or barrier analysis.
 */
#ifndef MULIB_KERNEL_UNIFORMITY_H
#define MULIB_KERNEL_UNIFORMITY_H
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
/* Capacities count bool elements. Output must not overlap parameters or code. */
static inline bool mulib_kernel_uniformity(mulib_kernel_ir ir,const bool *uniform_param,size_t param_cap,bool *out,size_t cap){
 size_t ob,pb,cb;
 if(!mulib_kernel_validate(ir)||!out||cap<ir.count||param_cap<ir.param_count||(!uniform_param&&ir.param_count)||
    !mulib_media_size_mul(ir.count,sizeof *out,&ob)||!mulib_media_size_mul(ir.param_count,sizeof *uniform_param,&pb)||
    !mulib_media_size_mul(ir.count,sizeof *ir.code,&cb)||!mulib_kernel_ir_disjoint(out,ob,uniform_param,pb)||
    !mulib_kernel_ir_disjoint(out,ob,ir.code,cb))return false;
 for(size_t i=0;i<ir.count;i++){mulib_kernel_inst x=ir.code[i];unsigned a=mulib_kernel_ir_arity(x.op);out[i]=x.op==MULIB_KERNEL_PARAM?uniform_param[x.a]:x.op==MULIB_KERNEL_CONST?true:out[x.a]&&(a<2||out[x.b])&&(a<3||out[x.c]);}return true;
}
#ifdef __cplusplus
}
#endif
#endif
