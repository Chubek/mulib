/* MuLIB: timing-propagate. C11 single-header kernel.
 * Define MULIB_IMPL_TIMING_PROPAGATE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_TIMING_PROPAGATE_H
#define MULIB_TIMING_PROPAGATE_H
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
#include "mulib-timing-graph.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_timing_propagate(size_t n,const mulib_graph_weighted_edge *edges,size_t m,const double *input,const double *output,double *arrive,double *required,size_t *pred);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TIMING_PROPAGATE) && !defined(MULIB_TIMING_PROPAGATE_H_IMPLEMENTED)
#define MULIB_TIMING_PROPAGATE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_timing_propagate(size_t n,const mulib_graph_weighted_edge *edges,size_t m,const double *input,const double *output,double *arrive,double *required,size_t *pred){if((!edges&&m)||(!input&&n)||(!output&&n)||(!arrive&&n)||(!required&&n)||(!pred&&n))return 0;size_t *order=(size_t*)mulib_storage_alloc(n,sizeof *order);if(!order)return 0;if(!mulib_timing_graph_order(n,edges,m,order)){free(order);return 0;}for(size_t i=0;i<n;i++){if(isnan(input[i])||isnan(output[i])){free(order);return 0;}arrive[i]=input[i];required[i]=output[i];pred[i]=MULIB_STORAGE_NONE;}for(size_t k=0;k<n;k++){size_t u=order[k];for(size_t j=0;j<m;j++)if(edges[j].from==u){size_t v=edges[j].to;double x=arrive[u]+edges[j].weight;if(x>arrive[v]){arrive[v]=x;pred[v]=u;}}}for(size_t k=n;k-->0;){size_t u=order[k];for(size_t j=0;j<m;j++)if(edges[j].from==u){double x=required[edges[j].to]-edges[j].weight;if(x<required[u])required[u]=x;}}free(order);return 1;}
#ifdef __cplusplus
}
#endif
#endif
