/* MuLIB / Amb3r: formal-counterexample. C11 single-header kernel.
 * Define MULIB_IMPL_FORMAL_COUNTEREXAMPLE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FORMAL_COUNTEREXAMPLE_H
#define MULIB_FORMAL_COUNTEREXAMPLE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_formal_counterexample(const size_t *parent,size_t states,size_t bad,size_t *path,size_t capacity,size_t *length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FORMAL_COUNTEREXAMPLE) && !defined(MULIB_FORMAL_COUNTEREXAMPLE_H_IMPLEMENTED)
#define MULIB_FORMAL_COUNTEREXAMPLE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_formal_counterexample(const size_t *parent,size_t states,size_t bad,size_t *path,size_t capacity,size_t *length){if(!parent||bad>=states||!length)return 0;size_t n=0,i=bad;while(i!=MA_NONE){if(i>=states||n==states)return 0;n++;i=parent[i];}*length=n;if(!path||n>capacity)return 0;i=bad;while(n){path[--n]=i;i=parent[i];}return 1;}
#ifdef __cplusplus
}
#endif
#endif
