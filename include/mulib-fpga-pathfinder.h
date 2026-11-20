/* MuLIB: fpga-pathfinder. C11 single-header kernel.
 * Define MULIB_IMPL_FPGA_PATHFINDER in exactly one implementation translation unit.
 * Profile: Bounded two-terminal graph routing with negotiated congestion history; not a complete architecture-aware router.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FPGA_PATHFINDER_H
#define MULIB_FPGA_PATHFINDER_H
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
#include "mulib-fpga-rrgraph.h"
#include "mulib-route-negotiated.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {size_t source,target;} mulib_fpga_net;
int mulib_fpga_pathfinder(const mulib_fpga_rrgraph *g,const mulib_fpga_net *nets,size_t count,const unsigned *capacity,size_t rounds,size_t *routes,size_t stride,size_t *lengths);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FPGA_PATHFINDER) && !defined(MULIB_FPGA_PATHFINDER_H_IMPLEMENTED)
#define MULIB_FPGA_PATHFINDER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fpga_pathfinder(const mulib_fpga_rrgraph *g,const mulib_fpga_net *nets,size_t count,const unsigned *capacity,size_t rounds,size_t *routes,size_t stride,size_t *lengths){size_t total;if(!g||(!nets&&count)||(!capacity&&g->n)||(!routes&&count)||(!lengths&&count)||!stride||!mulib_storage_size_mul(count,stride,&total))return 0;for(size_t i=0;i<g->n;i++)if(!capacity[i]||g->edges[i].from>=g->vertices||g->edges[i].to>=g->vertices)return 0;unsigned *usage=(unsigned*)mulib_storage_alloc(g->n,sizeof *usage);double *history=(double*)mulib_storage_alloc(g->n,sizeof *history);if(!usage||!history){free(usage);free(history);return 0;}int ok=0;for(size_t k=0;k<rounds;k++){memset(usage,0,g->n*sizeof *usage);for(size_t i=0;i<count;i++){size_t *path=routes+i*stride;if(!mulib_route_negotiated(g->vertices,g->edges,g->n,usage,capacity,history,nets[i].source,nets[i].target,path,stride,lengths+i))goto end;for(size_t j=1;j<lengths[i];j++){size_t e=0;while(e<g->n&&(g->edges[e].from!=path[j-1]||g->edges[e].to!=path[j]))e++;if(e==g->n||usage[e]==UINT_MAX)goto end;usage[e]++;}}ok=1;for(size_t i=0;i<g->n;i++)if(usage[i]>capacity[i]){ok=0;history[i]+=(double)usage[i]-capacity[i];}if(ok)break;}end:free(usage);free(history);return ok;}
#ifdef __cplusplus
}
#endif
#endif
