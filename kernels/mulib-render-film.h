/** @file mulib-render-film.h
 * HDR sample accumulation and resolve.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No AOV multiplexing; not thread-safe.
 */
#ifndef MULIB_RENDER_FILM_H
#define MULIB_RENDER_FILM_H
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
typedef struct {mulib_mesh_vec3 sum;uint64_t samples;} mulib_render_film_pixel;
static inline bool mulib_render_film_add(mulib_render_film_pixel *p,mulib_mesh_vec3 value){if(!p||p->samples==UINT64_MAX||!mulib_mesh_finite(value))return false;mulib_mesh_vec3 s=mulib_mesh_add(p->sum,value);if(!mulib_mesh_finite(s))return false;p->sum=s;p->samples++;return true;}
static inline mulib_mesh_vec3 mulib_render_film_resolve(mulib_render_film_pixel p){return p.samples?mulib_mesh_scale(p.sum,1.0/p.samples):mulib_mesh_v(0,0,0);}
#ifdef __cplusplus
}
#endif
#endif
