/** @file mulib-mesh-attributes.h
 * Checked dense per-element attribute channels.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_MESH_ATTRIBUTES_H
#define MULIB_MESH_ATTRIBUTES_H
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
typedef struct {float *values;size_t elements,components,capacity;} mulib_mesh_attribute;
static inline bool mulib_mesh_attribute_valid(mulib_mesh_attribute a){size_t n;return a.values&&a.components&&a.capacity<=SIZE_MAX/sizeof *a.values&&mulib_media_size_mul(a.elements,a.components,&n)&&n<=a.capacity;}
static inline bool mulib_mesh_attribute_set(mulib_mesh_attribute a,size_t i,const float *v){if(!mulib_mesh_attribute_valid(a)||i>=a.elements||!v)return false;memmove(a.values+i*a.components,v,a.components*sizeof *v);return true;}
#ifdef __cplusplus
}
#endif
#endif
