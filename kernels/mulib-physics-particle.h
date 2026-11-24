/** @file mulib-physics-particle.h
 * Particle advection, exponential drag and lifetime expiration.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_PHYSICS_PARTICLE_H
#define MULIB_PHYSICS_PARTICLE_H
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
typedef struct {mulib_mesh_vec3 position,velocity;double life;} mulib_physics_particle;
static inline size_t mulib_physics_particle_step(mulib_physics_particle *p,size_t n,mulib_mesh_vec3 a,double dt,double drag){if(!p||!mulib_mesh_finite(a)||!isfinite(dt)||dt<=0||!isfinite(drag)||drag<0)return 0;size_t alive=0;double damp=exp(-drag*dt);for(size_t i=0;i<n;i++)if(p[i].life>0){p[i].life=fmax(0,p[i].life-dt);p[i].velocity=mulib_mesh_scale(mulib_mesh_add(p[i].velocity,mulib_mesh_scale(a,dt)),damp);p[i].position=mulib_mesh_add(p[i].position,mulib_mesh_scale(p[i].velocity,dt));alive+=p[i].life>0;}return alive;}
#ifdef __cplusplus
}
#endif
#endif
