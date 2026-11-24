/** @file mulib-physics-joint.h
 * Position-based distance joint.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Does not update velocities or support angular constraints.
 */
#ifndef MULIB_PHYSICS_JOINT_H
#define MULIB_PHYSICS_JOINT_H
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
/* Separate body objects. Overflow preserves both; coincident points are left
 * unchanged because no constraint direction is available. */
static inline bool mulib_physics_joint_distance(mulib_physics_rigidbody *a,mulib_physics_rigidbody *b,double rest,double stiffness){if(!a||!b||!mulib_mesh_disjoint(a,sizeof *a,b,sizeof *b)||!mulib_physics_rigidbody_valid(*a)||!mulib_physics_rigidbody_valid(*b)||!isfinite(rest)||rest<0||!isfinite(stiffness)||stiffness<0||stiffness>1)return false;mulib_mesh_vec3 d=mulib_mesh_sub(b->position,a->position);double l=mulib_mesh_length(d),w=a->inv_mass+b->inv_mass;if(!isfinite(l)||!isfinite(w))return false;if(!w||l<1e-12)return true;d=mulib_mesh_scale(mulib_mesh_unit(d),((l-rest)/w)*stiffness);mulib_mesh_vec3 x=mulib_mesh_add(a->position,mulib_mesh_scale(d,a->inv_mass)),y=mulib_mesh_sub(b->position,mulib_mesh_scale(d,b->inv_mass));if(!mulib_mesh_finite(x)||!mulib_mesh_finite(y))return false;a->position=x;b->position=y;return true;}
#ifdef __cplusplus
}
#endif
#endif
