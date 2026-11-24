/** @file mulib-kernel-legalize.h
 * Finite constant folding as a first legalization pass.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No backend-specific capability lowering; verify float semantic parity.
 */
#ifndef MULIB_KERNEL_LEGALIZE_H
#define MULIB_KERNEL_LEGALIZE_H
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
/* Fold finite constant expressions in-place; preserve original SSA IDs. */
static inline bool mulib_kernel_legalize_fold(mulib_kernel_inst *code,mulib_kernel_ir ir){if(!code||code!=ir.code||!mulib_kernel_validate(ir))return false;for(size_t i=0;i<ir.count;i++){mulib_kernel_inst x=code[i];unsigned n=mulib_kernel_ir_arity(x.op);if(!n||code[x.a].op!=MULIB_KERNEL_CONST||(n>1&&code[x.b].op!=MULIB_KERNEL_CONST)||(n>2&&code[x.c].op!=MULIB_KERNEL_CONST))continue;float z=mulib_kernel_validate_eval_inst(x,code[x.a].literal,n>1?code[x.b].literal:0,n>2?code[x.c].literal:0);if(isfinite(z)){x.op=MULIB_KERNEL_CONST;x.a=x.b=x.c=0;x.literal=z;code[i]=x;}}return true;}
#ifdef __cplusplus
}
#endif
#endif
