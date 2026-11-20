/** @file mulib-anim-morph.h
 * Weighted blend-shape delta application.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Dense blend-shape deltas; no sparse targets or topology remapping.
 */
#ifndef MULIB_ANIM_MORPH_H
#define MULIB_ANIM_MORPH_H
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
/* out/base[vertices], delta[targets*vertices] (target-major), weights[targets].
 * Exact out==base is allowed; all other overlaps with out rejected. Two passes
 * preserve out on nonfinite inputs/intermediate overflow. O(vertices*targets). */
static inline bool mulib_anim_morph(mulib_mesh_vec3 *out,const mulib_mesh_vec3 *base,const mulib_mesh_vec3 *delta,const double *weights,size_t vertices,size_t targets){
 size_t n;if(((!out||!base)&&vertices)||(!delta&&targets&&vertices)||(!weights&&targets)||vertices>SIZE_MAX/sizeof *out||targets>SIZE_MAX/sizeof *weights||!mulib_media_size_mul(vertices,targets,&n)||n>SIZE_MAX/sizeof *delta)return false;
 size_t bytes=vertices*sizeof *out;
 if((out!=base&&!mulib_mesh_disjoint(out,bytes,base,bytes))||!mulib_mesh_disjoint(out,bytes,delta,n*sizeof *delta)||!mulib_mesh_disjoint(out,bytes,weights,targets*sizeof *weights))return false;
 for(size_t j=0;j<targets;j++)if(!isfinite(weights[j]))return false;
 for(unsigned pass=0;pass<2;pass++)for(size_t i=0;i<vertices;i++){mulib_mesh_vec3 p=base[i];if(!mulib_mesh_finite(p))return false;for(size_t j=0;j<targets;j++){mulib_mesh_vec3 d=delta[j*vertices+i];if(!mulib_mesh_finite(d))return false;p=mulib_mesh_add(p,mulib_mesh_scale(d,weights[j]));if(!mulib_mesh_finite(p))return false;}if(pass)out[i]=p;}
 return true;
}
#ifdef __cplusplus
}
#endif
#endif
