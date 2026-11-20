/* MuLIB: canvas-layer. C11 single-header kernel.
 * Define MULIB_IMPL_CANVAS_LAYER in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_CANVAS_LAYER_H
#define MULIB_CANVAS_LAYER_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#include "mulib-graph-core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t id;int64_t z;unsigned visible,locked;} mulib_canvas_layer;
size_t mulib_canvas_layer_order(const mulib_canvas_layer *layers,size_t n,size_t *out,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CANVAS_LAYER) && !defined(MULIB_CANVAS_LAYER_H_IMPLEMENTED)
#define MULIB_CANVAS_LAYER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_canvas_layer_order(const mulib_canvas_layer *layers,size_t n,size_t *out,size_t capacity){if((!layers&&n)||(!out&&capacity))return MULIB_STORAGE_NONE;size_t count=0;for(size_t i=0;i<n;i++)if(layers[i].visible){if(count>=capacity)return MULIB_STORAGE_NONE;size_t j=count;while(j&&layers[out[j-1]].z>layers[i].z){out[j]=out[j-1];j--;}out[j]=i;count++;}return count;}
#ifdef __cplusplus
}
#endif
#endif
