/* MuLIB / Amb3r: formal-miter. C11 single-header kernel.
 * Define MULIB_IMPL_FORMAL_MITER in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FORMAL_MITER_H
#define MULIB_FORMAL_MITER_H
#include "mulib-amb3r-base.h"
#include "mulib-synth-aig-rewrite.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_formal_miter(ma_aig *g,const size_t *a,const size_t *b,size_t n,size_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FORMAL_MITER) && !defined(MULIB_FORMAL_MITER_H_IMPLEMENTED)
#define MULIB_FORMAL_MITER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_formal_miter(ma_aig *g,const size_t *a,const size_t *b,size_t n,size_t *out){if(!g||(!a&&n)||(!b&&n)||!out)return 0;size_t root=0,old=g->n;for(size_t i=0;i<n;i++){size_t x,y,z,w;if(!ma_aig_and(g,a[i],b[i]^1,&x)||!ma_aig_and(g,a[i]^1,b[i],&y)||!ma_aig_and(g,x^1,y^1,&z)||!ma_aig_and(g,root^1,z,&w)){g->n=old;return 0;}root=w^1;}*out=root;return 1;}
#ifdef __cplusplus
}
#endif
#endif
