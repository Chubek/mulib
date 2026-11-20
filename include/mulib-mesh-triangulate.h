/** @file mulib-mesh-triangulate.h
 * Ear clipping for simple planar polygons.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No exact predicates or holes; assumes non-self-intersection.
 */
#ifndef MULIB_MESH_TRIANGULATE_H
#define MULIB_MESH_TRIANGULATE_H
#include "mulib-mesh-core.h"
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
typedef struct {double x,y;} mulib_mesh_pt2;
static inline double mulib_mesh_tri_cross2(mulib_mesh_pt2 a,mulib_mesh_pt2 b,mulib_mesh_pt2 c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);}
/* Internal ear pass: validated disjoint spans only. out=NULL is a dry run. */
static inline bool mulib_mesh_triangulate_run(const mulib_mesh_pt2 *p,size_t n,uint32_t *scratch,mulib_mesh_tri *out,double sign){
 for(size_t i=0;i<n;i++)scratch[i]=(uint32_t)i;
 size_t m=n,k=0;
 while(m>3){
  bool found=false;
  for(size_t i=0;i<m;i++){
   uint32_t u=scratch[i?i-1:m-1],v=scratch[i],w=scratch[i+1==m?0:i+1];
   double c=sign*mulib_mesh_tri_cross2(p[u],p[v],p[w]);
   if(!isfinite(c))return false;
   if(c<=1e-15)continue;
   bool contain=false;
   for(size_t j=0;j<m;j++){
    uint32_t q=scratch[j];if(q==u||q==v||q==w)continue;
    double a=sign*mulib_mesh_tri_cross2(p[u],p[v],p[q]),b=sign*mulib_mesh_tri_cross2(p[v],p[w],p[q]),d=sign*mulib_mesh_tri_cross2(p[w],p[u],p[q]);
    if(!isfinite(a)||!isfinite(b)||!isfinite(d))return false;
    if(a>=0&&b>=0&&d>=0){contain=true;break;}
   }
   if(contain)continue;
   if(out){mulib_mesh_tri t={u,v,w};out[k]=t;}
   k++;memmove(scratch+i,scratch+i+1,(m-i-1)*sizeof *scratch);m--;found=true;break;
  }
  if(!found)return false;
 }
 double c=sign*mulib_mesh_tri_cross2(p[scratch[0]],p[scratch[1]],p[scratch[2]]);
 if(!isfinite(c)||c<=1e-15)return false;
 if(out){mulib_mesh_tri t={scratch[0],scratch[1],scratch[2]};out[k]=t;}
 return true;
}
/* Simple CCW or CW polygon, no holes. scratch_cap: uint32 elements >=n;
 * cap: triangle elements >=n-2. All spans disjoint. Invalid/self-crossing or
 * numerically degenerate input fails, preserving out (scratch may change).
 * Double predicates/absolute 1e-15 ear threshold are not robust CAD predicates.
 * Work can be O(n^3); caller must bound n for untrusted input. */
static inline bool mulib_mesh_triangulate(const mulib_mesh_pt2 *p,size_t n,uint32_t *scratch,size_t scratch_cap,mulib_mesh_tri *out,size_t cap){
 if(!p||!scratch||!out||n<3||n>UINT32_MAX||n>SIZE_MAX/sizeof *p||n>SIZE_MAX/sizeof *scratch||n-2>SIZE_MAX/sizeof *out||scratch_cap<n||cap<n-2)return false;
 if(!mulib_mesh_disjoint(p,n*sizeof *p,scratch,n*sizeof *scratch)||!mulib_mesh_disjoint(p,n*sizeof *p,out,(n-2)*sizeof *out)||!mulib_mesh_disjoint(scratch,n*sizeof *scratch,out,(n-2)*sizeof *out))return false;
 for(size_t i=0;i<n;i++)if(!isfinite(p[i].x)||!isfinite(p[i].y))return false;
 double area=0;
 for(size_t i=0;i<n;i++){
  size_t j=i+1==n?0:i+1;
  if(p[i].x==p[j].x&&p[i].y==p[j].y)return false;
  area+=p[i].x*p[j].y-p[j].x*p[i].y;
  if(!isfinite(area))return false;
  for(size_t k=i+1;k<n;k++){
   size_t l=k+1==n?0:k+1;if(k==j||l==i)continue;
   double a=mulib_mesh_tri_cross2(p[i],p[j],p[k]),b=mulib_mesh_tri_cross2(p[i],p[j],p[l]),c=mulib_mesh_tri_cross2(p[k],p[l],p[i]),d=mulib_mesh_tri_cross2(p[k],p[l],p[j]);
   if(!isfinite(a)||!isfinite(b)||!isfinite(c)||!isfinite(d))return false;
   bool straddle=((a>=0&&b<=0)||(a<=0&&b>=0))&&((c>=0&&d<=0)||(c<=0&&d>=0));
   bool bounds=fmax(p[i].x,p[j].x)>=fmin(p[k].x,p[l].x)&&fmax(p[k].x,p[l].x)>=fmin(p[i].x,p[j].x)&&fmax(p[i].y,p[j].y)>=fmin(p[k].y,p[l].y)&&fmax(p[k].y,p[l].y)>=fmin(p[i].y,p[j].y);
   if(straddle&&bounds)return false;
  }
 }
 if(fabs(area)<1e-15)return false;
 double sign=area>0?1:-1;
 if(!mulib_mesh_triangulate_run(p,n,scratch,NULL,sign))return false;
 return mulib_mesh_triangulate_run(p,n,scratch,out,sign);
}
#ifdef __cplusplus
}
#endif
#endif
