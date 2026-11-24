/** @file mulib-physics-broadphase.h
 * Sphere AABB overlap candidate generation.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: O(n²); validate bodies before use.
 */
#ifndef MULIB_PHYSICS_BROADPHASE_H
#define MULIB_PHYSICS_BROADPHASE_H
#include "mulib-mesh-core.h"
#include "mulib-physics-rigidbody.h"
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
typedef struct {size_t a,b;} mulib_physics_pair;
static inline size_t mulib_physics_broadphase(const mulib_physics_rigidbody *b,size_t n,mulib_physics_pair *out,size_t cap){if(!b)return 0;size_t count=0;for(size_t i=0;i<n;i++)for(size_t j=i+1;j<n;j++){if(!b[i].inv_mass&&!b[j].inv_mass)continue;mulib_mesh_vec3 d=mulib_mesh_sub(b[i].position,b[j].position);double r=b[i].radius+b[j].radius;if(fabs(d.x)<=r&&fabs(d.y)<=r&&fabs(d.z)<=r){if(out&&count<cap){out[count].a=i;out[count].b=j;}if(count==SIZE_MAX)return count;count++;}}return count;}
#ifdef __cplusplus
}
#endif
#endif
