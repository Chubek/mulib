/** @file mulib-render-bvhbuild.h
 * Balanced centroid-partition BVH builder.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Not SAH or Morton LBVH; caller provides 2N-1 nodes.
 */
#ifndef MULIB_RENDER_BVHBUILD_H
#define MULIB_RENDER_BVHBUILD_H
#include "mulib-mesh-core.h"
#include "mulib-render-bvh.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
static inline double mulib_render_bvhbuild_key(mulib_render_triangle t,unsigned axis){return axis==0?t.a.x/3+t.b.x/3+t.c.x/3:axis==1?t.a.y/3+t.b.y/3+t.c.y/3:t.a.z/3+t.b.z/3+t.c.z/3;}
static inline size_t mulib_render_bvhbuild_rec(mulib_render_scene s,mulib_render_bvh_node *nodes,size_t *used,size_t *ids,size_t first,size_t n){size_t id=(*used)++;mulib_render_bvh_node v={0};v.box=mulib_render_bvh_triangle(s.triangles[ids[first]]);for(size_t i=1;i<n;i++)v.box=mulib_render_bvh_union(v.box,mulib_render_bvh_triangle(s.triangles[ids[first+i]]));if(n<=4){v.first=first;v.count=n;nodes[id]=v;return id;}mulib_mesh_vec3 e=mulib_mesh_sub(v.box.hi,v.box.lo);unsigned a=e.x>=e.y&&e.x>=e.z?0:e.y>=e.z?1:2;/* partition around midpoint; use index median if degenerate */double split=mulib_render_bvhbuild_key(s.triangles[ids[first+n/2]],a);size_t l=first,r=first+n;while(l<r){if(mulib_render_bvhbuild_key(s.triangles[ids[l]],a)<split)l++;else{size_t z=ids[l];ids[l]=ids[--r];ids[r]=z;}}size_t k=l-first;if(k<n/4||k>n-n/4)k=n/2;v.left=mulib_render_bvhbuild_rec(s,nodes,used,ids,first,k);v.right=mulib_render_bvhbuild_rec(s,nodes,used,ids,first+k,n-k);nodes[id]=v;return id;}
/* cap: nodes, idcap: size_t elements. Arrays, used and triangle storage must
 * be disjoint. Bounds/capacity failures preserve outputs. rec is an internal
 * unchecked helper, not an untrusted-input entry point. */
static inline bool mulib_render_bvhbuild(mulib_render_scene s,mulib_render_bvh_node *nodes,size_t cap,size_t *ids,size_t idcap,size_t *used){if(!nodes||!ids||!used||!s.count||s.count>idcap||s.count>SIZE_MAX/2||cap<2*s.count-1||cap>SIZE_MAX/sizeof *nodes||idcap>SIZE_MAX/sizeof *ids||s.count>SIZE_MAX/sizeof *s.triangles)return false;const void *ptr[4]={nodes,ids,used,s.triangles};size_t bytes[4]={cap*sizeof *nodes,idcap*sizeof *ids,sizeof *used,s.count*sizeof *s.triangles};for(size_t i=0;i<4;i++)for(size_t j=0;j<i;j++)if(!mulib_mesh_disjoint(ptr[i],bytes[i],ptr[j],bytes[j]))return false;if(!mulib_render_scene_valid(s))return false;for(size_t i=0;i<s.count;i++)ids[i]=i;*used=0;mulib_render_bvhbuild_rec(s,nodes,used,ids,0,s.count);return true;}
#ifdef __cplusplus
}
#endif
#endif
