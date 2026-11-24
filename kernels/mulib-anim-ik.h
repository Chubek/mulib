/** @file mulib-anim-ik.h
 * FABRIK position-chain inverse kinematics.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: False may still return best-effort pose; no joint limits.
 */
#ifndef MULIB_ANIM_IK_H
#define MULIB_ANIM_IK_H
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
/* FABRIK: n positions, len_count double elements >=n-1, disjoint arrays.
 * Invalid arguments preserve p. Nonconvergence/numeric failure may leave a
 * finite best-effort pose; this is NOT transactional. No joint limits.
 * iterations<=4096; caller also budgets O(n*iterations) work. */
static inline bool mulib_anim_ik_fabrik(mulib_mesh_vec3 *p,const double *len,size_t len_count,size_t n,mulib_mesh_vec3 target,unsigned iterations,double tolerance){
 if(!p||!len||n<2||len_count<n-1||n>SIZE_MAX/sizeof *p||(n-1)>SIZE_MAX/sizeof *len||!mulib_mesh_disjoint(p,n*sizeof *p,len,(n-1)*sizeof *len)||!mulib_mesh_finite(target)||!isfinite(tolerance)||tolerance<0||!iterations||iterations>4096)return false;
 double sum=0;for(size_t i=0;i<n;i++){if(!mulib_mesh_finite(p[i]))return false;if(i+1<n){if(!isfinite(len[i])||len[i]<=0)return false;sum+=len[i];if(!isfinite(sum))return false;}}
 mulib_mesh_vec3 root=p[0],delta=mulib_mesh_sub(target,root);double distance=mulib_mesh_length(delta);
 if(!isfinite(distance))return false;
 if(distance>=sum){mulib_mesh_vec3 d=mulib_mesh_unit(delta);for(size_t i=1;i<n;i++){mulib_mesh_vec3 q=mulib_mesh_add(p[i-1],mulib_mesh_scale(d,len[i-1]));if(!mulib_mesh_finite(q))return false;p[i]=q;}return distance-sum<=tolerance;}
 for(unsigned k=0;k<iterations;k++){
  p[n-1]=target;
  for(size_t i=n-1;i;i--){delta=mulib_mesh_sub(p[i-1],p[i]);if(!mulib_mesh_finite(delta))return false;mulib_mesh_vec3 d=mulib_mesh_unit(delta);if(!mulib_mesh_length(d))d=mulib_mesh_v(1,0,0);mulib_mesh_vec3 q=mulib_mesh_add(p[i],mulib_mesh_scale(d,len[i-1]));if(!mulib_mesh_finite(q))return false;p[i-1]=q;}
  p[0]=root;
  for(size_t i=1;i<n;i++){delta=mulib_mesh_sub(p[i],p[i-1]);if(!mulib_mesh_finite(delta))return false;mulib_mesh_vec3 d=mulib_mesh_unit(delta);if(!mulib_mesh_length(d))d=mulib_mesh_v(1,0,0);mulib_mesh_vec3 q=mulib_mesh_add(p[i-1],mulib_mesh_scale(d,len[i-1]));if(!mulib_mesh_finite(q))return false;p[i]=q;}
  if(mulib_mesh_length(mulib_mesh_sub(p[n-1],target))<=tolerance)return true;
 }
 return false;
}
#ifdef __cplusplus
}
#endif
#endif
