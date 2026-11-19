/* MuLIB / Amb3r: formal-ltl. C11 single-header kernel.
 * Define MULIB_IMPL_FORMAL_LTL in exactly one implementation translation unit.
 * Profile: Temporal operators on given lasso traces; not general LTL model checking.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FORMAL_LTL_H
#define MULIB_FORMAL_LTL_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_formal_ltl_lasso(unsigned op,const unsigned char *p,const unsigned char *q,size_t n,size_t loop,unsigned char *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FORMAL_LTL) && !defined(MULIB_FORMAL_LTL_H_IMPLEMENTED)
#define MULIB_FORMAL_LTL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_formal_ltl_lasso(unsigned op,const unsigned char *p,const unsigned char *q,size_t n,size_t loop,unsigned char *out){if(!n||loop>=n||!p||!out||op>3||(op==3&&!q))return 0;unsigned char *a=(unsigned char*)ma_alloc(n,1),*b=(unsigned char*)ma_alloc(n,1);if(!a||!b){free(a);free(b);return 0;}for(size_t i=0;i<n;i++)a[i]=op==2?1:0;for(size_t k=0;k<=n;k++){for(size_t i=0;i<n;i++){unsigned char next=a[i+1<n?i+1:loop];b[i]=(unsigned char)(op==0?!!p[i+1<n?i+1:loop]:op==1?(p[i]||next):op==2?(p[i]&&next):(q[i]||(p[i]&&next)));}if(op==0||!memcmp(a,b,n)){memcpy(out,b,n);free(a);free(b);return 1;}unsigned char *t=a;a=b;b=t;}free(a);free(b);return 0;}
#ifdef __cplusplus
}
#endif
#endif
