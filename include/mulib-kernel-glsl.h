/** @file mulib-kernel-glsl.h
 * Emit GLSL scalar function from validated SSA.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No compute entry point, descriptors or driver compilation; locale must be C.
 */
#ifndef MULIB_KERNEL_GLSL_H
#define MULIB_KERNEL_GLSL_H
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
#include <locale.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Bounded text generation, not a shader compiler. Define smaller budgets before
 * inclusion for untrusted inputs. Emission requires decimal-dot locale; caller
 * must serialize global locale changes. IO failure can leave partial text.
 * GPU NaN/Inf, subnormal, signed-zero and rounding behavior is target-dependent;
 * only finite, nonexceptional expression semantics are intended here. */
#ifndef MULIB_KERNEL_EMIT_MAX_INSTRUCTIONS
#define MULIB_KERNEL_EMIT_MAX_INSTRUCTIONS 65536u
#endif
#ifndef MULIB_KERNEL_EMIT_MAX_PARAMS
#define MULIB_KERNEL_EMIT_MAX_PARAMS 1024u
#endif
static inline bool mulib_kernel_glsl_emittable(mulib_kernel_ir ir){
 const struct lconv *lc=localeconv();
 return ir.count<=MULIB_KERNEL_EMIT_MAX_INSTRUCTIONS&&ir.param_count<=MULIB_KERNEL_EMIT_MAX_PARAMS&&
        lc&&lc->decimal_point&&!strcmp(lc->decimal_point,".")&&mulib_kernel_validate(ir);
}
/* Low-level expression writer: IDs/types must already have been validated. */
static inline bool mulib_kernel_glsl_expr(FILE *f,mulib_kernel_inst x,bool wgsl){if(!f||mulib_kernel_ir_arity(x.op)==UINT_MAX||(x.op==MULIB_KERNEL_CONST&&(!isfinite(x.literal)||(x.type!=MULIB_KERNEL_F32&&x.type!=MULIB_KERNEL_BOOL)||(x.type==MULIB_KERNEL_BOOL&&x.literal!=0&&x.literal!=1))))return false;switch(x.op){case MULIB_KERNEL_PARAM:return fprintf(f,"p%u",(unsigned)x.a)>=0;case MULIB_KERNEL_CONST:if(x.type==MULIB_KERNEL_BOOL)return fputs(x.literal!=0?"true":"false",f)>=0;return fprintf(f,"%.9e",(double)x.literal)>=0;case MULIB_KERNEL_NEG:return fprintf(f,"-v%u",(unsigned)x.a)>=0;case MULIB_KERNEL_MIN:case MULIB_KERNEL_MAX:return fprintf(f,"%s(v%u,v%u)",x.op==MULIB_KERNEL_MIN?"min":"max",(unsigned)x.a,(unsigned)x.b)>=0;case MULIB_KERNEL_SELECT:return wgsl?fprintf(f,"select(v%u,v%u,v%u)",(unsigned)x.c,(unsigned)x.b,(unsigned)x.a)>=0:fprintf(f,"v%u ? v%u : v%u",(unsigned)x.a,(unsigned)x.b,(unsigned)x.c)>=0;default:{char op=x.op==MULIB_KERNEL_ADD?'+':x.op==MULIB_KERNEL_SUB?'-':x.op==MULIB_KERNEL_MUL?'*':x.op==MULIB_KERNEL_DIV?'/':'<';return fprintf(f,"v%u %c v%u",(unsigned)x.a,op,(unsigned)x.b)>=0;}}}
static inline bool mulib_kernel_glsl_emit(FILE *f,mulib_kernel_ir ir){if(!f||!mulib_kernel_glsl_emittable(ir)||fprintf(f,"%s mulib_kernel(",ir.code[ir.result].type==MULIB_KERNEL_BOOL?"bool":"float")<0)return false;for(uint32_t i=0;i<ir.param_count;i++)if(fprintf(f,"%sfloat p%u",i?", ":"",(unsigned)i)<0)return false;if(fputs(") {\n",f)<0)return false;for(size_t i=0;i<ir.count;i++){if(fprintf(f,"  %s v%zu = ",ir.code[i].type==MULIB_KERNEL_BOOL?"bool":"float",i)<0||!mulib_kernel_glsl_expr(f,ir.code[i],false)||fputs(";\n",f)<0)return false;}return fprintf(f,"  return v%u;\n}\n",(unsigned)ir.result)>=0;}
#ifdef __cplusplus
}
#endif
#endif
