/* MuLIB: hdl-elaborate. C11 single-header kernel.
 * Define MULIB_IMPL_HDL_ELABORATE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HDL_ELABORATE_H
#define MULIB_HDL_ELABORATE_H
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
#include "mulib-graph-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_hdl_elaborate_order(size_t modules,const mulib_graph_weighted_edge *dependencies,size_t n,size_t *out);
int mulib_hdl_elaborate_generate(int64_t start,int64_t stop,int64_t stride,int64_t *values,size_t cap,size_t *count);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HDL_ELABORATE) && !defined(MULIB_HDL_ELABORATE_H_IMPLEMENTED)
#define MULIB_HDL_ELABORATE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hdl_elaborate_order(size_t modules,const mulib_graph_weighted_edge *dependencies,size_t n,size_t *out){return mulib_graph_edges_toposort(modules,dependencies,n,out);}
int mulib_hdl_elaborate_generate(int64_t start,int64_t stop,int64_t stride,int64_t *values,size_t cap,size_t *count){if(!stride||!count||(stride>0&&start>stop)||(stride<0&&start<stop))return 0;size_t n=0;int64_t i=start;while(stride>0?i<stop:i>stop){if(n==cap||!values)return 0;values[n++]=i;if((stride>0&&i>INT64_MAX-stride)||(stride<0&&i<INT64_MIN-stride))break;i+=stride;}*count=n;return 1;}
#ifdef __cplusplus
}
#endif
#endif
