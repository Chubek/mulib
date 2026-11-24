/** @file mulib-geom-cloner.h
 * Grid and radial instance positions; linear is grid with two dimensions =1.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_GEOM_CLONER_H
#define MULIB_GEOM_CLONER_H
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
/* cap is vec3 elements. Numeric/capacity errors preserve out. */
static inline bool mulib_geom_cloner_grid(mulib_mesh_vec3 *out,size_t cap,unsigned nx,unsigned ny,unsigned nz,mulib_mesh_vec3 step){size_t n;if(!out||!nx||!ny||!nz||!mulib_mesh_finite(step)||!mulib_media_size_mul(nx,ny,&n)||!mulib_media_size_mul(n,nz,&n)||n>cap||n>SIZE_MAX/sizeof *out||!mulib_mesh_finite(mulib_mesh_v((nx-1)*step.x,(ny-1)*step.y,(nz-1)*step.z)))return false;size_t i=0;for(unsigned z=0;z<nz;z++)for(unsigned y=0;y<ny;y++)for(unsigned x=0;x<nx;x++)out[i++]=mulib_mesh_v(x*step.x,y*step.y,z*step.z);return true;}
/* out[n] vec3; radius in coordinate units, phase in radians. */
static inline bool mulib_geom_cloner_radial(mulib_mesh_vec3 *out,size_t n,double radius,double phase){if(!out||!n||n>SIZE_MAX/sizeof *out||!isfinite(radius)||radius<0||!isfinite(phase))return false;phase=remainder(phase,6.283185307179586);for(size_t i=0;i<n;i++){double a=phase+6.283185307179586*((double)i/n);out[i]=mulib_mesh_v(radius*cos(a),radius*sin(a),0);}return true;}
#ifdef __cplusplus
}
#endif
#endif
