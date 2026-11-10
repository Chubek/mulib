/* MuLIB compiler tools: Verify and execute a bounded bytecode program without implicit host calls.
 * ISO C11 single-header module. Define MULIB_IMPL_BYTECODE_EVAL in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_BYTECODE_EVAL_H
#define MULIB_BYTECODE_EVAL_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-bytecode-verify.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-bytecode-verify.h"
int mulib_bytecode_eval(const mulib_bc_instruction_t *code,size_t count,size_t locals,size_t steps,mulib_bc_result_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BYTECODE_EVAL_H */
#if defined(MULIB_IMPL_BYTECODE_EVAL) && !defined(MULIB_BYTECODE_EVAL_IMPLEMENTED)
#define MULIB_BYTECODE_EVAL_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_bytecode_eval(const mulib_bc_instruction_t *code,size_t n,size_t locals,size_t steps,mulib_bc_result_t *out){size_t peak,bad=0;
if(!out)return 0;
if(!mulib_bytecode_verify(code,n,locals,&peak,&bad)){
*out=(mulib_bc_result_t){MULIB_BC_INVALID,bad,0,0};
return 0;
}
return mulib_bytecode_run(code,n,locals,peak,steps,out)==MULIB_BC_OK;
}
#ifdef __cplusplus
}
#endif
#endif
