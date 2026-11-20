/** @file mulib-color-grade.h
 * ASC-CDL-style slope, offset, power and saturation.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Unclamped positive output; specify CDL clamp mode for interchange.
 */
#ifndef MULIB_COLOR_GRADE_H
#define MULIB_COLOR_GRADE_H
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
typedef struct {float slope[3],offset[3],power[3],saturation;} mulib_color_grade_cdl;
static inline bool mulib_color_grade_apply(mulib_img_view d,mulib_img_view s,mulib_color_grade_cdl p){if(!mulib_img_same(d,s)||!mulib_img_pointwise_alias(d,s)||!isfinite(p.saturation)||p.saturation<0)return false;for(int i=0;i<3;i++)if(!isfinite(p.slope[i])||!isfinite(p.offset[i])||!isfinite(p.power[i])||p.slope[i]<0||p.power[i]<=0)return false;for(size_t y=0;y<s.height;y++)for(size_t x=0;x<s.width;x++){mulib_img_rgba c=mulib_img_get(s,x,y);float v[3]={c.r,c.g,c.b};for(int i=0;i<3;i++)v[i]=powf(fmaxf(0,v[i]*p.slope[i]+p.offset[i]),p.power[i]);c.r=v[0];c.g=v[1];c.b=v[2];float l=mulib_img_luma(c);c.r=l+(c.r-l)*p.saturation;c.g=l+(c.g-l)*p.saturation;c.b=l+(c.b-l)*p.saturation;mulib_img_put(d,x,y,c);}return true;}
#ifdef __cplusplus
}
#endif
#endif
