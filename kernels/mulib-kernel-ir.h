/** @file mulib-kernel-ir.h
 * Typed straight-line SSA expression IR with explicit result.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Executable subset is f32 and bool; no CFG, memory instructions or vector values.
 */
#ifndef MULIB_KERNEL_IR_H
#define MULIB_KERNEL_IR_H
#include "mulib-media-core.h"
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
typedef enum {MULIB_KERNEL_F32=1,MULIB_KERNEL_I32,MULIB_KERNEL_U32,MULIB_KERNEL_BOOL} mulib_kernel_type;
typedef enum {MULIB_KERNEL_PARAM,MULIB_KERNEL_CONST,MULIB_KERNEL_ADD,MULIB_KERNEL_SUB,MULIB_KERNEL_MUL,MULIB_KERNEL_DIV,MULIB_KERNEL_MIN,MULIB_KERNEL_MAX,MULIB_KERNEL_NEG,MULIB_KERNEL_LT,MULIB_KERNEL_SELECT} mulib_kernel_op;
typedef struct {mulib_kernel_op op;mulib_kernel_type type;uint32_t a,b,c;float literal;} mulib_kernel_inst;
typedef struct {const mulib_kernel_inst *code;size_t count;uint32_t result,param_count;} mulib_kernel_ir;
/* All arrays are borrowed. Count is the actual readable instruction capacity;
 * param_count is the required number of f32 parameters (including unused ones).
 * f32 arithmetic follows C/libm: NaN/Inf may be produced; MIN/MAX use fminf/
 * fmaxf (one NaN selects the other operand). No fast-math parity is promised.
 * Integer enum values are representation helpers, not executable IR types. */
/* Conservative byte-range check for ordinary flat-address C targets. Invalid
 * or wrapping ranges are not disjoint. It does not establish pointer ownership. */
static inline bool mulib_kernel_ir_disjoint(const void *a,size_t an,const void *b,size_t bn){
 if(!an||!bn)return true;
 if(!a||!b||an>UINTPTR_MAX||bn>UINTPTR_MAX)return false;
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 if(x>UINTPTR_MAX-an||y>UINTPTR_MAX-bn)return false;
 return x<=y?an<=y-x:bn<=x-y;
}
static inline unsigned mulib_kernel_ir_arity(mulib_kernel_op op){switch(op){case MULIB_KERNEL_PARAM:case MULIB_KERNEL_CONST:return 0;case MULIB_KERNEL_NEG:return 1;case MULIB_KERNEL_SELECT:return 3;case MULIB_KERNEL_ADD:case MULIB_KERNEL_SUB:case MULIB_KERNEL_MUL:case MULIB_KERNEL_DIV:case MULIB_KERNEL_MIN:case MULIB_KERNEL_MAX:case MULIB_KERNEL_LT:return 2;default:return UINT_MAX;}}
#ifdef __cplusplus
}
#endif
#endif
