/** @file mulib-texbytecode.h
 * @brief Safe stack-bytecode arithmetic interpreter with bounded instruction steps.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_TEXBYTECODE once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-texbytecode.md for scope and limitations.
 */
#ifndef MULIB_TEXBYTECODE_H
#define MULIB_TEXBYTECODE_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { mulib_texbytecode_PUSH=1,mulib_texbytecode_ADD=2,mulib_texbytecode_SUB=3,mulib_texbytecode_MUL=4,mulib_texbytecode_DIV=5,mulib_texbytecode_HALT=6 };
typedef struct mulib_texbytecode_insn { unsigned opcode;double literal; } mulib_texbytecode_insn_t;
int mulib_texbytecode_run(const mulib_texbytecode_insn_t *program,size_t length,size_t max_steps,double *result);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TEXBYTECODE_H */
#if defined(MULIB_IMPL_TEXBYTECODE) && !defined(MULIB_TEXBYTECODE_H_IMPLEMENTED)
#define MULIB_TEXBYTECODE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_texbytecode_run(const mulib_texbytecode_insn_t *p,size_t n,size_t steps,double *out){if(!p||!out||!steps)return 0;double stack[256];size_t top=0;
 for(size_t pc=0;pc<n&&pc<steps;pc++){unsigned op=p[pc].opcode;if(op==mulib_texbytecode_HALT){if(top!=1)return 0;*out=stack[0];return 1;}if(op==mulib_texbytecode_PUSH){if(top>=256||!isfinite(p[pc].literal))return 0;stack[top++]=p[pc].literal;continue;}if(top<2)return 0;double b=stack[--top],a=stack[--top],v=0;switch(op){case mulib_texbytecode_ADD:v=a+b;break;case mulib_texbytecode_SUB:v=a-b;break;case mulib_texbytecode_MUL:v=a*b;break;case mulib_texbytecode_DIV:if(b==0)return 0;v=a/b;break;default:return 0;}if(!isfinite(v))return 0;stack[top++]=v;}
 return 0;}
#ifdef __cplusplus
}
#endif
#endif
