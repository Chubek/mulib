/** @file mulib-kernel-wgsl.h
 * Emit WGSL scalar function with typed let bindings.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No entry point, resources, or shader validation by native compiler.
 */
#ifndef MULIB_KERNEL_WGSL_H
#define MULIB_KERNEL_WGSL_H
#include "mulib-kernel-ir.h"
#include "mulib-kernel-glsl.h"
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
/* Same locale, work budgets and finite-intermediate restrictions as GLSL. */
static inline bool mulib_kernel_wgsl_emit(FILE *f,mulib_kernel_ir ir){if(!f||!mulib_kernel_glsl_emittable(ir)||fputs("fn mulib_kernel(",f)<0)return false;for(uint32_t i=0;i<ir.param_count;i++)if(fprintf(f,"%sp%u: f32",i?", ":"",(unsigned)i)<0)return false;if(fprintf(f,") -> %s {\n",ir.code[ir.result].type==MULIB_KERNEL_BOOL?"bool":"f32")<0)return false;for(size_t i=0;i<ir.count;i++){if(fprintf(f,"  let v%zu: %s = ",i,ir.code[i].type==MULIB_KERNEL_BOOL?"bool":"f32")<0||!mulib_kernel_glsl_expr(f,ir.code[i],true)||fputs(";\n",f)<0)return false;}return fprintf(f,"  return v%u;\n}\n",(unsigned)ir.result)>=0;}
#ifdef __cplusplus
}
#endif
#endif
