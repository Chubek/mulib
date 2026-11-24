/** @file mulib-anim-skeleton.h
 * Parent-ordered skeleton forward point transformation.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No matrix palette generation.
 */
#ifndef MULIB_ANIM_SKELETON_H
#define MULIB_ANIM_SKELETON_H
#include "mulib-mesh-core.h"
#include "mulib-anim-transform.h"
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
/* Parent precedes child, SIZE_MAX means root. Global point via chain; exact even with nonuniform scale. */
static inline bool mulib_anim_skeleton_point(const mulib_anim_transform *local,const size_t *parent,size_t n,size_t bone,mulib_mesh_vec3 p,mulib_mesh_vec3 *out){if(!local||!parent||!out||bone>=n)return false;size_t i=bone;for(;;){p=mulib_anim_transform_point(local[i],p);size_t q=parent[i];if(q==SIZE_MAX)break;if(q>=i)return false;i=q;}if(!mulib_mesh_finite(p))return false;*out=p;return true;}
#ifdef __cplusplus
}
#endif
#endif
