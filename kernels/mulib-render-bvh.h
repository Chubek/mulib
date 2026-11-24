/** @file mulib-render-bvh.h
 * AABB slab intersection and caller-stack BVH traversal.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Builder-generated tree assumed; checked traversal distinguishes invalid/miss.
 */
#ifndef MULIB_RENDER_BVH_H
#define MULIB_RENDER_BVH_H
#include "mulib-mesh-core.h"
#include "mulib-render-scene.h"
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
typedef struct {mulib_mesh_vec3 lo,hi;} mulib_render_aabb;
typedef struct {mulib_render_aabb box;size_t left,right,first,count;} mulib_render_bvh_node;
static inline mulib_render_aabb mulib_render_bvh_triangle(mulib_render_triangle t){mulib_render_aabb b={mulib_mesh_v(fmin(t.a.x,fmin(t.b.x,t.c.x)),fmin(t.a.y,fmin(t.b.y,t.c.y)),fmin(t.a.z,fmin(t.b.z,t.c.z))),mulib_mesh_v(fmax(t.a.x,fmax(t.b.x,t.c.x)),fmax(t.a.y,fmax(t.b.y,t.c.y)),fmax(t.a.z,fmax(t.b.z,t.c.z)))};return b;}
static inline mulib_render_aabb mulib_render_bvh_union(mulib_render_aabb a,mulib_render_aabb b){mulib_render_aabb c={mulib_mesh_v(fmin(a.lo.x,b.lo.x),fmin(a.lo.y,b.lo.y),fmin(a.lo.z,b.lo.z)),mulib_mesh_v(fmax(a.hi.x,b.hi.x),fmax(a.hi.y,b.hi.y),fmax(a.hi.z,b.hi.z))};return c;}
static inline bool mulib_render_bvh_box_valid(mulib_render_aabb b){return mulib_mesh_finite(b.lo)&&mulib_mesh_finite(b.hi)&&b.lo.x<=b.hi.x&&b.lo.y<=b.hi.y&&b.lo.z<=b.hi.z;}
static inline bool mulib_render_bvh_box_hit(mulib_render_aabb b,mulib_render_ray r,double lo,double hi){if(!mulib_render_bvh_box_valid(b)||!mulib_mesh_finite(r.origin)||!mulib_mesh_finite(r.direction)||!(lo<=hi))return false;double p[3]={r.origin.x,r.origin.y,r.origin.z},d[3]={r.direction.x,r.direction.y,r.direction.z},mn[3]={b.lo.x,b.lo.y,b.lo.z},mx[3]={b.hi.x,b.hi.y,b.hi.z};for(int i=0;i<3;i++){if(d[i]==0){if(p[i]<mn[i]||p[i]>mx[i])return false;continue;}double a=(mn[i]-p[i])/d[i],z=(mx[i]-p[i])/d[i];lo=fmax(lo,fmin(a,z));hi=fmin(hi,fmax(a,z));if(lo>hi)return false;}return true;}
typedef enum {MULIB_RENDER_BVH_INVALID=-1,MULIB_RENDER_BVH_MISS=0,MULIB_RENDER_BVH_HIT=1} mulib_render_bvh_result;
/* nn, index_count, cap are element counts. Nodes/indices must originate from
 * builder and remain immutable. Stack/out must be disjoint from inputs and
 * each other. At most nn node visits: malformed cycles cannot hang traversal.
 * out unchanged on MISS/INVALID; stack is scratch. Numeric tri overflow is miss.
 * This validates visited nodes, not full tree structure or conservative bounds. */
static inline mulib_render_bvh_result mulib_render_bvh_hit_checked(mulib_render_scene s,const mulib_render_bvh_node *nodes,size_t nn,const size_t *indices,size_t index_count,mulib_render_ray r,double min,double max,size_t *stack,size_t cap,mulib_render_hit *out){
 if(!nodes||!nn||!indices||!stack||!cap||!out||!s.triangles||nn>SIZE_MAX/sizeof *nodes||index_count>SIZE_MAX/sizeof *indices||cap>SIZE_MAX/sizeof *stack||s.count>SIZE_MAX/sizeof *s.triangles||!mulib_mesh_finite(r.origin)||!mulib_mesh_finite(r.direction)||!(min<=max)||(r.direction.x==0&&r.direction.y==0&&r.direction.z==0))return MULIB_RENDER_BVH_INVALID;
 const void *input[3]={nodes,indices,s.triangles};size_t bytes[3]={nn*sizeof *nodes,index_count*sizeof *indices,s.count*sizeof *s.triangles};
 if(!mulib_mesh_disjoint(stack,cap*sizeof *stack,out,sizeof *out))return MULIB_RENDER_BVH_INVALID;
 for(size_t i=0;i<3;i++)if(!mulib_mesh_disjoint(stack,cap*sizeof *stack,input[i],bytes[i])||!mulib_mesh_disjoint(out,sizeof *out,input[i],bytes[i]))return MULIB_RENDER_BVH_INVALID;
 size_t n=1,steps=0;stack[0]=0;bool found=false;mulib_render_hit best={0};
 while(n){
  if(steps++==nn)return MULIB_RENDER_BVH_INVALID;
  size_t i=stack[--n];if(i>=nn)return MULIB_RENDER_BVH_INVALID;
  mulib_render_bvh_node b=nodes[i];if(!mulib_render_bvh_box_valid(b.box))return MULIB_RENDER_BVH_INVALID;
  if(b.count){if(b.first>index_count||b.count>index_count-b.first)return MULIB_RENDER_BVH_INVALID;}
  else if(b.left>=nn||b.right>=nn||b.left==i||b.right==i||b.left==b.right)return MULIB_RENDER_BVH_INVALID;
  if(!mulib_render_bvh_box_hit(b.box,r,min,max))continue;
  if(b.count){
   for(size_t j=0;j<b.count;j++){size_t k=indices[b.first+j];if(k>=s.count)return MULIB_RENDER_BVH_INVALID;mulib_render_hit h;if(mulib_render_scene_tri_hit(s.triangles[k],r,min,max,&h)){h.triangle=k;best=h;max=h.t;found=true;}}
  }else{if(cap-n<2)return MULIB_RENDER_BVH_INVALID;stack[n++]=b.right;stack[n++]=b.left;}
 }
 if(found)*out=best;
 return found?MULIB_RENDER_BVH_HIT:MULIB_RENDER_BVH_MISS;
}
/* Compatibility convenience: false conflates miss/invalid; prefer checked API. */
static inline bool mulib_render_bvh_hit(mulib_render_scene s,const mulib_render_bvh_node *nodes,size_t nn,const size_t *indices,size_t index_count,mulib_render_ray r,double min,double max,size_t *stack,size_t cap,mulib_render_hit *out){return mulib_render_bvh_hit_checked(s,nodes,nn,indices,index_count,r,min,max,stack,cap,out)==MULIB_RENDER_BVH_HIT;}
#ifdef __cplusplus
}
#endif
#endif
