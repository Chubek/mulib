/** @file mulib-physics-solver.h
 * Normal collision impulse and positional correction.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Single contact, no friction or angular effects.
 */
#ifndef MULIB_PHYSICS_SOLVER_H
#define MULIB_PHYSICS_SOLVER_H
#include "mulib-mesh-core.h"
#include "mulib-physics-narrowphase.h"
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
/* n bodies; c is separate from their storage. Normal must be finite/unit.
 * Invalid input or numeric overflow preserves both bodies and the contact. */
static inline bool mulib_physics_solver_contact(mulib_physics_rigidbody *b,size_t n,mulib_physics_contact *c,double correction){
 if(!b||!c||n>SIZE_MAX/sizeof *b||!mulib_mesh_disjoint(b,n*sizeof *b,c,sizeof *c)||c->a>=n||c->b>=n||c->a==c->b||!isfinite(correction)||correction<0||correction>1||!isfinite(c->depth)||c->depth<0||!mulib_mesh_finite(c->normal)||fabs(mulib_mesh_length(c->normal)-1)>1e-6)return false;
 mulib_physics_rigidbody a=b[c->a],d=b[c->b];
 if(!mulib_physics_rigidbody_valid(a)||!mulib_physics_rigidbody_valid(d))return false;
 double w=a.inv_mass+d.inv_mass;if(!isfinite(w))return false;if(!w){c->impulse=0;return true;}
 double v=mulib_mesh_dot(mulib_mesh_sub(d.velocity,a.velocity),c->normal);
 if(!isfinite(v))return false;
 double j=v<0?-(1+fmin(a.restitution,d.restitution))*(v/w):0;
 if(!isfinite(j))return false;
 mulib_mesh_vec3 imp=mulib_mesh_scale(c->normal,j),pos=mulib_mesh_scale(c->normal,(fmax(0,c->depth-.001)/w)*correction);
 a.velocity=mulib_mesh_sub(a.velocity,mulib_mesh_scale(imp,a.inv_mass));d.velocity=mulib_mesh_add(d.velocity,mulib_mesh_scale(imp,d.inv_mass));
 a.position=mulib_mesh_sub(a.position,mulib_mesh_scale(pos,a.inv_mass));d.position=mulib_mesh_add(d.position,mulib_mesh_scale(pos,d.inv_mass));
 if(!mulib_physics_rigidbody_valid(a)||!mulib_physics_rigidbody_valid(d))return false;
 b[c->a]=a;b[c->b]=d;c->impulse=j;return true;
}
#ifdef __cplusplus
}
#endif
#endif
