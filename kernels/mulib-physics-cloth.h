/** @file mulib-physics-cloth.h
 * Position-based cloth stretch constraint projection with pinning.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No integration, bending or self-collision.
 */
#ifndef MULIB_PHYSICS_CLOTH_H
#define MULIB_PHYSICS_CLOTH_H
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
typedef struct {size_t a,b;double rest,stiffness;} mulib_physics_cloth_link;
static inline bool mulib_physics_cloth_project(mulib_mesh_vec3 *p,const double *inv_mass,size_t n,const mulib_physics_cloth_link *links,size_t count,unsigned iterations){if(!p||!inv_mass||(!links&&count)||!iterations||iterations>256)return false;for(size_t i=0;i<n;i++)if(!mulib_mesh_finite(p[i])||!isfinite(inv_mass[i])||inv_mass[i]<0)return false;for(size_t i=0;i<count;i++)if(links[i].a>=n||links[i].b>=n||links[i].a==links[i].b||!isfinite(links[i].rest)||links[i].rest<0||!isfinite(links[i].stiffness)||links[i].stiffness<0||links[i].stiffness>1)return false;for(unsigned k=0;k<iterations;k++)for(size_t i=0;i<count;i++){mulib_physics_cloth_link c=links[i];double a=inv_mass[c.a],b=inv_mass[c.b];mulib_mesh_vec3 d=mulib_mesh_sub(p[c.b],p[c.a]);double l=mulib_mesh_length(d);if(a+b>0&&l>1e-12){d=mulib_mesh_scale(d,(l-c.rest)*c.stiffness/(l*(a+b)));p[c.a]=mulib_mesh_add(p[c.a],mulib_mesh_scale(d,a));p[c.b]=mulib_mesh_sub(p[c.b],mulib_mesh_scale(d,b));}}return true;}
#ifdef __cplusplus
}
#endif
#endif
