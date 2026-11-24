/** @file mulib-geom-isosurface.h
 * Marching tetrahedron cell polygonization.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Requires grid traversal, consistent winding and vertex welding.
 */
#ifndef MULIB_GEOM_ISOSURFACE_H
#define MULIB_GEOM_ISOSURFACE_H
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
/* Exactly four p/v inputs; out_cap is vec3 elements (3 or 6 required).
 * Returns triangle count; 0 conflates empty/invalid/insufficient capacity.
 * Output preserved on failure. Unwelded/unoriented; no grid traversal.
 * Extreme field ranges are scaled before division to avoid inf/inf. */
static inline size_t mulib_geom_isosurface_tet(const mulib_mesh_vec3 p[4],const double v[4],double iso,mulib_mesh_vec3 *out,size_t out_cap){if(!p||!v||!out||!isfinite(iso))return 0;int a[4],b[4],na=0,nb=0;for(int i=0;i<4;i++){if(!isfinite(v[i])||!mulib_mesh_finite(p[i]))return 0;if(v[i]<iso)a[na++]=i;else b[nb++]=i;}if(!na||!nb)return 0;size_t triangles=na==2?2:1;if(out_cap<3*triangles)return 0;mulib_mesh_vec3 q[4];int n=0;for(int i=0;i<na;i++)for(int j=0;j<nb;j++){double lo=v[a[i]],hi=v[b[j]],num=iso-lo,den=hi-lo;if(!isfinite(den)){num=iso*.5-lo*.5;den=hi*.5-lo*.5;}double t=num/den;if(!isfinite(t)||t<0||t>1)return 0;q[n]=mulib_mesh_add(mulib_mesh_scale(p[a[i]],1-t),mulib_mesh_scale(p[b[j]],t));if(!mulib_mesh_finite(q[n]))return 0;n++;}out[0]=q[0];out[1]=q[1];out[2]=q[2];if(n==4){out[3]=q[1];out[4]=q[3];out[5]=q[2];}return triangles;}
#ifdef __cplusplus
}
#endif
#endif
