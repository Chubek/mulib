/** @file mulib-render-sampling.h
 * Deterministic RNG, radical inverse and cosine-weighted hemisphere samples.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_RENDER_SAMPLING_H
#define MULIB_RENDER_SAMPLING_H
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
/* PCG-style xorshift/rotate output from a 64-bit LCG. Explicit seed/state. */
static inline uint32_t mulib_render_sampling_u32(uint64_t *state){uint64_t old=*state;*state=old*UINT64_C(6364136223846793005)+UINT64_C(1442695040888963407);uint32_t x=(uint32_t)(((old>>18)^old)>>27),r=(uint32_t)(old>>59);return (x>>r)|(x<<((-r)&31));}
static inline double mulib_render_sampling_uniform(uint64_t *state){return (mulib_render_sampling_u32(state)+.5)/4294967296.0;}
static inline double mulib_render_sampling_radical(uint64_t n,unsigned base){if(base<2)return NAN;double f=1.0/base,s=0;while(n){s+=(n%base)*f;n/=base;f/=base;}return s;}
static inline mulib_mesh_vec3 mulib_render_sampling_cosine(mulib_mesh_vec3 n,double u,double v){double r=sqrt(fmax(0,fmin(1,u))),a=6.283185307179586*v;n=mulib_mesh_unit(n);mulib_mesh_vec3 t=mulib_mesh_unit(mulib_mesh_cross(fabs(n.z)<.999?mulib_mesh_v(0,0,1):mulib_mesh_v(0,1,0),n)),b=mulib_mesh_cross(n,t);return mulib_mesh_unit(mulib_mesh_add(mulib_mesh_scale(n,sqrt(fmax(0,1-r*r))),mulib_mesh_add(mulib_mesh_scale(t,r*cos(a)),mulib_mesh_scale(b,r*sin(a)))));}
#ifdef __cplusplus
}
#endif
#endif
