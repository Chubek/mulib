/** @file mulib-physics-world.h
 * Sphere-world integrator with iterative contact resolution.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: O(n²), no CCD or persistent broadphase; partial state on numeric failure.
 */
#ifndef MULIB_PHYSICS_WORLD_H
#define MULIB_PHYSICS_WORLD_H
#include "mulib-mesh-core.h"
#include "mulib-physics-solver.h"
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
static inline bool mulib_physics_world_step(mulib_physics_rigidbody *b,size_t n,mulib_mesh_vec3 gravity,double dt,unsigned iterations){if((!b&&n)||!iterations||iterations>128||!isfinite(dt)||dt<=0||!mulib_mesh_finite(gravity))return false;for(size_t i=0;i<n;i++)if(!mulib_physics_rigidbody_valid(b[i]))return false;for(size_t i=0;i<n;i++)if(!mulib_physics_rigidbody_step(b+i,gravity,dt))return false;for(unsigned k=0;k<iterations;k++)for(size_t i=0;i<n;i++)for(size_t j=i+1;j<n;j++){mulib_physics_contact c;if(mulib_physics_narrowphase_sphere(b,n,i,j,&c)&&!mulib_physics_solver_contact(b,n,&c,.8))return false;}return true;}
#ifdef __cplusplus
}
#endif
#endif
