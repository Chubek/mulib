/* MuLIB: synth-arithmetic. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_ARITHMETIC in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_ARITHMETIC_H
#define MULIB_SYNTH_ARITHMETIC_H
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

int mulib_synth_arithmetic_adder(mulib_synth_aig_graph *g,const size_t *a,const size_t *b,size_t n,size_t cin,size_t *out,size_t *cout);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_ARITHMETIC) && !defined(MULIB_SYNTH_ARITHMETIC_H_IMPLEMENTED)
#define MULIB_SYNTH_ARITHMETIC_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_arithmetic_adder(mulib_synth_aig_graph *g,const size_t *a,const size_t *b,size_t n,size_t cin,size_t *out,size_t *cout){if(!g||!a||!b||!out||!cout||cin/2>g->n)return 0;size_t old=g->n;for(size_t i=0;i<n;i++){size_t u,v,w,x,y,z;if(!mulib_synth_aig_graph_and(g,a[i],b[i]^1,&u)||!mulib_synth_aig_graph_and(g,a[i]^1,b[i],&v)||!mulib_synth_aig_graph_and(g,u^1,v^1,&w)||!mulib_synth_aig_graph_and(g,w^1,cin^1,&x)||!mulib_synth_aig_graph_and(g,w,cin,&y)||!mulib_synth_aig_graph_and(g,x^1,y^1,&z))goto fail;out[i]=z^1;if(!mulib_synth_aig_graph_and(g,a[i],b[i],&u)||!mulib_synth_aig_graph_and(g,w^1,cin,&v)||!mulib_synth_aig_graph_and(g,u^1,v^1,&w))goto fail;cin=w^1;}*cout=cin;return 1;fail:g->n=old;return 0;}
#ifdef __cplusplus
}
#endif
#endif
