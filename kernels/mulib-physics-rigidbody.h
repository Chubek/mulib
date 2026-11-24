/** @file mulib-physics-rigidbody.h
 * Semi-implicit Euler for translational sphere rigid bodies.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No rotation, torque, inertia tensor or angular momentum.
 */
#ifndef MULIB_PHYSICS_RIGIDBODY_H
#define MULIB_PHYSICS_RIGIDBODY_H
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
typedef struct {mulib_mesh_vec3 position,velocity,force;double inv_mass,radius,restitution;} mulib_physics_rigidbody;
static inline bool mulib_physics_rigidbody_valid(mulib_physics_rigidbody b){return mulib_mesh_finite(b.position)&&mulib_mesh_finite(b.velocity)&&mulib_mesh_finite(b.force)&&isfinite(b.inv_mass)&&b.inv_mass>=0&&isfinite(b.radius)&&b.radius>0&&isfinite(b.restitution)&&b.restitution>=0&&b.restitution<=1;}
static inline bool mulib_physics_rigidbody_step(mulib_physics_rigidbody *b,mulib_mesh_vec3 gravity,double dt){if(!b||!mulib_physics_rigidbody_valid(*b)||!mulib_mesh_finite(gravity)||!isfinite(dt)||dt<=0)return false;if(!b->inv_mass)return true;mulib_physics_rigidbody x=*b;x.velocity=mulib_mesh_add(x.velocity,mulib_mesh_scale(mulib_mesh_add(gravity,mulib_mesh_scale(x.force,x.inv_mass)),dt));x.position=mulib_mesh_add(x.position,mulib_mesh_scale(x.velocity,dt));x.force=mulib_mesh_v(0,0,0);if(!mulib_physics_rigidbody_valid(x))return false;*b=x;return true;}
#ifdef __cplusplus
}
#endif
#endif
