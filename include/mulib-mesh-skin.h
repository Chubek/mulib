/** @file mulib-mesh-skin.h
 * Four-weight normalized linear blend skinning.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_MESH_SKIN_H
#define MULIB_MESH_SKIN_H
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
typedef struct {double m[12];} mulib_mesh_skin_matrix;
typedef struct {uint32_t bone[4];float weight[4];} mulib_mesh_skin_weights;
static inline bool mulib_mesh_skin_vertex(mulib_mesh_vec3 p,mulib_mesh_skin_weights w,const mulib_mesh_skin_matrix *bones,size_t n,mulib_mesh_vec3 *out){if(!bones||!out||!mulib_mesh_finite(p))return false;mulib_mesh_vec3 z=mulib_mesh_v(0,0,0);double sum=0;for(int i=0;i<4;i++){float f=w.weight[i];if(!isfinite(f)||f<0)return false;if(!f)continue;if(w.bone[i]>=n)return false;const double *a=bones[w.bone[i]].m;mulib_mesh_vec3 q=mulib_mesh_v(a[0]*p.x+a[1]*p.y+a[2]*p.z+a[3],a[4]*p.x+a[5]*p.y+a[6]*p.z+a[7],a[8]*p.x+a[9]*p.y+a[10]*p.z+a[11]);z=mulib_mesh_add(z,mulib_mesh_scale(q,f));sum+=f;}if(!(sum>0)||!mulib_mesh_finite(z))return false;*out=mulib_mesh_scale(z,1/sum);return true;}
#ifdef __cplusplus
}
#endif
#endif
