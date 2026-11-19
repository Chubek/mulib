/* MuLIB / Amb3r: formal-ctl. C11 single-header kernel.
 * Define MULIB_IMPL_FORMAL_CTL in exactly one implementation translation unit.
 * Profile: Explicit-state fixed-point CTL operators, not a full expression parser/model-checker frontend.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FORMAL_CTL_H
#define MULIB_FORMAL_CTL_H
#include "mulib-amb3r-base.h"
#include "mulib-formal-transition.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_formal_ctl(const ma_transition *s,unsigned op,const unsigned char *p,const unsigned char *q,unsigned char *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FORMAL_CTL) && !defined(MULIB_FORMAL_CTL_H_IMPLEMENTED)
#define MULIB_FORMAL_CTL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_formal_ctl(const ma_transition *s,unsigned op,const unsigned char *p,const unsigned char *q,unsigned char *out){if(!mulib_formal_transition_valid(s)||!p||!out||op>5||((op==2||op==3)&&!q))return 0;size_t n=s->states;unsigned char *a=(unsigned char*)ma_alloc(n,1),*b=(unsigned char*)ma_alloc(n,1);if(!a||!b){free(a);free(b);return 0;}for(size_t i=0;i<n;i++)a[i]=op==2||op==3?!!q[i]:!!p[i];int universal=(op&1);for(size_t step=0;step<=n;step++){for(size_t i=0;i<n;i++){int next=universal;if(s->offsets[i]==s->offsets[i+1])next=a[i];for(size_t k=s->offsets[i];k<s->offsets[i+1];k++){unsigned char v=a[s->targets[k]];if(universal)next=next&&v;else next=next||v;}b[i]=(unsigned char)(op<2?next:op<4?(q[i]||(p[i]&&next)):(p[i]&&next));}if(op<2||!memcmp(a,b,n)){memcpy(out,b,n);free(a);free(b);return 1;}unsigned char *t=a;a=b;b=t;}free(a);free(b);return 0;}
#ifdef __cplusplus
}
#endif
#endif
