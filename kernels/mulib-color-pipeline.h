/** @file mulib-color-pipeline.h
 * Fused per-pixel pipeline of caller-supplied color stages.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Callbacks trusted and synchronous.
 */
#ifndef MULIB_COLOR_PIPELINE_H
#define MULIB_COLOR_PIPELINE_H
#include "mulib-img-core.h"
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
typedef mulib_img_rgba (*mulib_color_pipeline_fn)(void*,mulib_img_rgba);
typedef struct {mulib_color_pipeline_fn eval;void *ctx;} mulib_color_pipeline_stage;
/* n is the accessible stage count. Stages/contexts must remain valid and
 * immutable during calls; callbacks may not access mutable image storage.
 * Exact in-place mapping is supported; shifted overlap is rejected. */
static inline bool mulib_color_pipeline_run(mulib_img_view d,mulib_img_view s,const mulib_color_pipeline_stage *st,size_t n){if(!mulib_img_same(d,s)||!mulib_img_pointwise_alias(d,s)||!mulib_img_disjoint_array(d,st,n,sizeof *st))return false;for(size_t i=0;i<n;i++)if(!st[i].eval)return false;for(size_t y=0;y<s.height;y++)for(size_t x=0;x<s.width;x++){mulib_img_rgba c=mulib_img_get(s,x,y);for(size_t i=0;i<n;i++)c=st[i].eval(st[i].ctx,c);mulib_img_put(d,x,y,c);}return true;}
#ifdef __cplusplus
}
#endif
#endif
