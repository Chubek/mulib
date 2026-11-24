/** @file mulib-anim-curve.h
 * Step, linear and cubic Hermite animation interpolation.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Tangents in value per tick.
 */
#ifndef MULIB_ANIM_CURVE_H
#define MULIB_ANIM_CURVE_H
#include "mulib-mesh-core.h"
#include "mulib-anim-keyframe.h"
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
typedef enum {MULIB_ANIM_STEP,MULIB_ANIM_LINEAR,MULIB_ANIM_HERMITE} mulib_anim_curve_mode;
static inline bool mulib_anim_curve_eval(const mulib_anim_key *k,size_t n,int64_t t,mulib_anim_curve_mode mode,double *out){
 if(!out||!mulib_anim_keyframe_valid(k,n)||mode<MULIB_ANIM_STEP||mode>MULIB_ANIM_HERMITE)return false;
 size_t i=mulib_anim_keyframe_floor(k,n,t);
 if(i==SIZE_MAX){*out=k[0].value;return true;}
 if(i>=n)return false;
 if(i==n-1||mode==MULIB_ANIM_STEP||t==k[i].tick){*out=k[i].value;return true;}
 /* Unsigned subtraction is exact over the entire sorted int64 tick range,
  * independent of long double precision (which may equal double). */
 double dt=(double)((uint64_t)k[i+1].tick-(uint64_t)k[i].tick);
 double u=(double)((uint64_t)t-(uint64_t)k[i].tick)/dt,z;
 if(mode==MULIB_ANIM_LINEAR)z=(1-u)*k[i].value+u*k[i+1].value;
 else{double u2=u*u,u3=u2*u;z=(2*u3-3*u2+1)*k[i].value+(u3-2*u2+u)*dt*k[i].out_tangent+(-2*u3+3*u2)*k[i+1].value+(u3-u2)*dt*k[i+1].in_tangent;}
 if(!isfinite(z))return false;
 *out=z;return true;
}
#ifdef __cplusplus
}
#endif
#endif
