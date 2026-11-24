/** @file mulib-physics-narrowphase.h
 * Sphere-sphere normal and penetration depth.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No convex or mesh collision.
 */
#ifndef MULIB_PHYSICS_NARROWPHASE_H
#define MULIB_PHYSICS_NARROWPHASE_H
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
typedef struct {size_t a,b;mulib_mesh_vec3 normal;double depth,impulse;} mulib_physics_contact;
/* false means miss OR invalid/unrepresentable geometry. out unchanged. */
static inline bool mulib_physics_narrowphase_sphere(const mulib_physics_rigidbody *b,size_t n,size_t a,size_t c,mulib_physics_contact *out){if(!b||!out||n>SIZE_MAX/sizeof *b||a>=n||c>=n||a==c||!mulib_physics_rigidbody_valid(b[a])||!mulib_physics_rigidbody_valid(b[c]))return false;mulib_mesh_vec3 d=mulib_mesh_sub(b[c].position,b[a].position);double l=mulib_mesh_length(d),r=b[a].radius+b[c].radius;if(!isfinite(l)||!isfinite(r)||l>=r)return false;mulib_physics_contact t={a,c,l>0?mulib_mesh_unit(d):mulib_mesh_v(1,0,0),r-l,0};*out=t;return true;}
#ifdef __cplusplus
}
#endif
#endif
