/* MuLIB: fpga-rrgraph. C11 single-header kernel.
 * Define MULIB_IMPL_FPGA_RRGRAPH in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FPGA_RRGRAPH_H
#define MULIB_FPGA_RRGRAPH_H
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
#include "mulib-fpga-grid.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {size_t vertices,n,cap;mulib_graph_weighted_edge *edges;} mulib_fpga_rrgraph;
int mulib_fpga_rrgraph_grid(size_t w,size_t h,double cost,mulib_fpga_rrgraph *out);
void mulib_fpga_rrgraph_free(mulib_fpga_rrgraph *g);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FPGA_RRGRAPH) && !defined(MULIB_FPGA_RRGRAPH_H_IMPLEMENTED)
#define MULIB_FPGA_RRGRAPH_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fpga_rrgraph_grid(size_t w,size_t h,double cost,mulib_fpga_rrgraph *out){size_t n;if(!w||!h||!out||out->edges||cost<0||!isfinite(cost)||!mulib_storage_size_mul(w,h,&n))return 0;mulib_fpga_rrgraph g={0};g.vertices=n;for(size_t i=0;i<n;i++){size_t adj[4],k=mulib_fpga_grid_neighbors(w,h,i,adj);for(size_t j=0;j<k;j++){mulib_graph_weighted_edge e={i,adj[j],cost};if(!mulib_storage_push((void**)&g.edges,&g.n,&g.cap,sizeof e,&e)){free(g.edges);return 0;}}}*out=g;return 1;}
void mulib_fpga_rrgraph_free(mulib_fpga_rrgraph *g){if(g){free(g->edges);memset(g,0,sizeof *g);}}
#ifdef __cplusplus
}
#endif
#endif
