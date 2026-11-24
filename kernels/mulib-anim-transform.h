/** @file mulib-anim-transform.h
 * TRS point transformation and animation interpolation.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No matrix/shear decomposition.
 */
#ifndef MULIB_ANIM_TRANSFORM_H
#define MULIB_ANIM_TRANSFORM_H
#include "mulib-mesh-core.h"
#include "mulib-anim-quaternion.h"
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
typedef struct {mulib_mesh_vec3 translation,scale;mulib_anim_quat rotation;} mulib_anim_transform;
static inline mulib_mesh_vec3 mulib_anim_transform_point(mulib_anim_transform t,mulib_mesh_vec3 p){p=mulib_mesh_v(p.x*t.scale.x,p.y*t.scale.y,p.z*t.scale.z);return mulib_mesh_add(t.translation,mulib_anim_quaternion_rotate(t.rotation,p));}
static inline mulib_anim_transform mulib_anim_transform_lerp(mulib_anim_transform a,mulib_anim_transform b,double u){mulib_anim_transform t={mulib_mesh_add(mulib_mesh_scale(a.translation,1-u),mulib_mesh_scale(b.translation,u)),mulib_mesh_add(mulib_mesh_scale(a.scale,1-u),mulib_mesh_scale(b.scale,u)),mulib_anim_quaternion_slerp(a.rotation,b.rotation,u)};return t;}
#ifdef __cplusplus
}
#endif
#endif
