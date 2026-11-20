/** @file mulib-kernel-fusion.h
 * Fuse two pure SSA expressions by parameter substitution and ID remapping.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Out and map require disjoint buffers; no side-effect semantics.
 */
#ifndef MULIB_KERNEL_FUSION_H
#define MULIB_KERNEL_FUSION_H
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
/* Arbitrary second expression with one parameter; substitute first result for its parameter loads. */
/* cap counts instructions; map_cap counts uint32_t entries. All writable
 * ranges must be disjoint from each other and both source instruction ranges.
 * Worst-case a.count+b.count capacity is required, even for substituted loads. */
static inline bool mulib_kernel_fusion(mulib_kernel_ir a,mulib_kernel_ir b,mulib_kernel_inst *out,size_t cap,uint32_t *map,size_t map_cap,mulib_kernel_ir *result){
 size_t total,ob,mb,ab,bb;
 if(!mulib_kernel_validate(a)||!mulib_kernel_validate(b)||b.param_count!=1||a.code[a.result].type!=MULIB_KERNEL_F32||!out||!map||!result||map_cap<b.count||
    !mulib_media_size_add(a.count,b.count,&total)||total>cap||total>UINT32_MAX||
    !mulib_media_size_mul(total,sizeof *out,&ob)||!mulib_media_size_mul(b.count,sizeof *map,&mb)||
    !mulib_media_size_mul(a.count,sizeof *a.code,&ab)||!mulib_media_size_mul(b.count,sizeof *b.code,&bb)||
    !mulib_kernel_ir_disjoint(out,ob,map,mb)||!mulib_kernel_ir_disjoint(out,ob,result,sizeof *result)||
    !mulib_kernel_ir_disjoint(map,mb,result,sizeof *result)||
    !mulib_kernel_ir_disjoint(out,ob,a.code,ab)||!mulib_kernel_ir_disjoint(out,ob,b.code,bb)||
    !mulib_kernel_ir_disjoint(map,mb,a.code,ab)||!mulib_kernel_ir_disjoint(map,mb,b.code,bb)||
    !mulib_kernel_ir_disjoint(result,sizeof *result,a.code,ab)||!mulib_kernel_ir_disjoint(result,sizeof *result,b.code,bb))return false;
 memcpy(out,a.code,ab);size_t n=a.count;
 for(size_t i=0;i<b.count;i++){mulib_kernel_inst x=b.code[i];if(x.op==MULIB_KERNEL_PARAM){map[i]=a.result;continue;}unsigned ar=mulib_kernel_ir_arity(x.op);if(ar)x.a=map[x.a];if(ar>1)x.b=map[x.b];if(ar>2)x.c=map[x.c];map[i]=(uint32_t)n;out[n++]=x;}
 mulib_kernel_ir r={out,n,map[b.result],a.param_count};*result=r;return true;
}
#ifdef __cplusplus
}
#endif
#endif
