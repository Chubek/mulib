/** @file mulib-anim-quaternion.h
 * Quaternion normalization, multiplication, shortest-path SLERP and vector rotation.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_ANIM_QUATERNION_H
#define MULIB_ANIM_QUATERNION_H
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
typedef struct {double x,y,z,w;} mulib_anim_quat;
/* Zero/nonfinite input maps to identity; finite extreme magnitudes normalize
 * after scaling, rather than accidentally mapping to identity on norm overflow. */
static inline mulib_anim_quat mulib_anim_quaternion_unit(mulib_anim_quat q){double s=fmax(fmax(fabs(q.x),fabs(q.y)),fmax(fabs(q.z),fabs(q.w)));if(isfinite(q.x)&&isfinite(q.y)&&isfinite(q.z)&&isfinite(q.w)&&s>0){q.x/=s;q.y/=s;q.z/=s;q.w/=s;double n=hypot(hypot(q.x,q.y),hypot(q.z,q.w));q.x/=n;q.y/=n;q.z/=n;q.w/=n;}else{q.x=q.y=q.z=0;q.w=1;}return q;}
static inline mulib_anim_quat mulib_anim_quaternion_mul(mulib_anim_quat a,mulib_anim_quat b){mulib_anim_quat q={a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};return q;}
static inline mulib_anim_quat mulib_anim_quaternion_slerp(mulib_anim_quat a,mulib_anim_quat b,double t){a=mulib_anim_quaternion_unit(a);b=mulib_anim_quaternion_unit(b);double d=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;if(d<0){d=-d;b.x=-b.x;b.y=-b.y;b.z=-b.z;b.w=-b.w;}double u=1-t,v=t;if(d<.9995){double th=acos(fmax(-1,fmin(1,d))),s=sin(th);u=sin((1-t)*th)/s;v=sin(t*th)/s;}mulib_anim_quat q={u*a.x+v*b.x,u*a.y+v*b.y,u*a.z+v*b.z,u*a.w+v*b.w};return mulib_anim_quaternion_unit(q);}
static inline mulib_mesh_vec3 mulib_anim_quaternion_rotate(mulib_anim_quat q,mulib_mesh_vec3 v){q=mulib_anim_quaternion_unit(q);mulib_mesh_vec3 u=mulib_mesh_v(q.x,q.y,q.z),t=mulib_mesh_scale(mulib_mesh_cross(u,v),2);return mulib_mesh_add(v,mulib_mesh_add(mulib_mesh_scale(t,q.w),mulib_mesh_cross(u,t)));}
#ifdef __cplusplus
}
#endif
#endif
