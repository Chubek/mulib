/** @file mulib-render-material.h
 * Validated linear-light PBR material parameter representation.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_RENDER_MATERIAL_H
#define MULIB_RENDER_MATERIAL_H
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
typedef struct {mulib_mesh_vec3 albedo,emission;double roughness,metallic;} mulib_render_material;
static inline bool mulib_render_material_valid(mulib_render_material m){return mulib_mesh_finite(m.albedo)&&mulib_mesh_finite(m.emission)&&m.albedo.x>=0&&m.albedo.y>=0&&m.albedo.z>=0&&m.albedo.x<=1&&m.albedo.y<=1&&m.albedo.z<=1&&m.emission.x>=0&&m.emission.y>=0&&m.emission.z>=0&&isfinite(m.roughness)&&m.roughness>=0&&m.roughness<=1&&isfinite(m.metallic)&&m.metallic>=0&&m.metallic<=1;}
#ifdef __cplusplus
}
#endif
#endif
