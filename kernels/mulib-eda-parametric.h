/* MuLIB: eda-parametric. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_PARAMETRIC in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_PARAMETRIC_H
#define MULIB_EDA_PARAMETRIC_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#include "mulib-num-core.h"
#include "mulib-graph-core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {unsigned op;size_t index;double literal;} mulib_eda_param_instruction;
int mulib_eda_parametric_eval(const mulib_eda_param_instruction *p,size_t n,const double *params,size_t np,double *result);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_PARAMETRIC) && !defined(MULIB_EDA_PARAMETRIC_H_IMPLEMENTED)
#define MULIB_EDA_PARAMETRIC_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_parametric_eval(const mulib_eda_param_instruction *p,size_t n,const double *params,size_t np,double *result){if(!p||!n||!result||(!params&&np))return 0;double *s=(double*)mulib_storage_alloc(n,sizeof(double));if(!s)return 0;size_t top=0;int ok=1;for(size_t i=0;i<n&&ok;i++){mulib_eda_param_instruction x=p[i];if(x.op==0)s[top++]=x.literal;else if(x.op==1){if(x.index>=np){ok=0;break;}s[top++]=params[x.index];}else if(x.op>=2&&x.op<=5&&top>=2){double b=s[--top],a=s[top-1];s[top-1]=x.op==2?a+b:x.op==3?a-b:x.op==4?a*b:b?a/b:NAN;}else ok=0;if(top&&!isfinite(s[top-1]))ok=0;}ok=ok&&top==1;if(ok)*result=s[0];free(s);return ok;}
#ifdef __cplusplus
}
#endif
#endif
