/* MuLIB: formal-miter. C11 single-header kernel.
 * Define MULIB_IMPL_FORMAL_MITER in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FORMAL_MITER_H
#define MULIB_FORMAL_MITER_H
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
#include "mulib-synth-aig-rewrite.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_formal_miter(mulib_synth_aig_graph *g,const size_t *a,const size_t *b,size_t n,size_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FORMAL_MITER) && !defined(MULIB_FORMAL_MITER_H_IMPLEMENTED)
#define MULIB_FORMAL_MITER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_formal_miter(mulib_synth_aig_graph *g,const size_t *a,const size_t *b,size_t n,size_t *out){if(!g||(!a&&n)||(!b&&n)||!out)return 0;size_t root=0,old=g->n;for(size_t i=0;i<n;i++){size_t x,y,z,w;if(!mulib_synth_aig_graph_and(g,a[i],b[i]^1,&x)||!mulib_synth_aig_graph_and(g,a[i]^1,b[i],&y)||!mulib_synth_aig_graph_and(g,x^1,y^1,&z)||!mulib_synth_aig_graph_and(g,root^1,z,&w)){g->n=old;return 0;}root=w^1;}*out=root;return 1;}
#ifdef __cplusplus
}
#endif
#endif
