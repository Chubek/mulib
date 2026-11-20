/** @file mulib-physics-manifold.h
 * Match previous sphere contacts by stable body indices and normal.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No manifold points or warm-start application.
 */
#ifndef MULIB_PHYSICS_MANIFOLD_H
#define MULIB_PHYSICS_MANIFOLD_H
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
static inline void mulib_physics_manifold_match(mulib_physics_contact *now,size_t n,const mulib_physics_contact *old,size_t count){if(!now||!old)return;for(size_t i=0;i<n;i++)for(size_t j=0;j<count;j++)if(now[i].a==old[j].a&&now[i].b==old[j].b&&mulib_mesh_dot(now[i].normal,old[j].normal)>.95){now[i].impulse=fmax(0,old[j].impulse);break;}}
#ifdef __cplusplus
}
#endif
#endif
