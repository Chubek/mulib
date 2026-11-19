/* MuLIB / Amb3r: canvas-layer. C11 single-header kernel.
 * Define MULIB_IMPL_CANVAS_LAYER in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_CANVAS_LAYER_H
#define MULIB_CANVAS_LAYER_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t id;int64_t z;unsigned visible,locked;} ma_canvas_layer;
size_t mulib_canvas_layer_order(const ma_canvas_layer *layers,size_t n,size_t *out,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CANVAS_LAYER) && !defined(MULIB_CANVAS_LAYER_H_IMPLEMENTED)
#define MULIB_CANVAS_LAYER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_canvas_layer_order(const ma_canvas_layer *layers,size_t n,size_t *out,size_t capacity){if((!layers&&n)||(!out&&capacity))return MA_NONE;size_t count=0;for(size_t i=0;i<n;i++)if(layers[i].visible){if(count>=capacity)return MA_NONE;size_t j=count;while(j&&layers[out[j-1]].z>layers[i].z){out[j]=out[j-1];j--;}out[j]=i;count++;}return count;}
#ifdef __cplusplus
}
#endif
#endif
