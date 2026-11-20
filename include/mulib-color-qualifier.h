/** @file mulib-color-qualifier.h
 * Soft luminance/saturation selection mask.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Hue and spatial cleanup not implemented.
 */
#ifndef MULIB_COLOR_QUALIFIER_H
#define MULIB_COLOR_QUALIFIER_H
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
static inline float mulib_color_qualifier_band(float x,float lo,float hi,float soft){if(hi<lo||soft<0)return 0;if(soft==0)return x>=lo&&x<=hi?1:0;float a=mulib_media_clampf((x-lo+soft)/soft,0,1),b=mulib_media_clampf((hi+soft-x)/soft,0,1);return a*b;}
static inline float mulib_color_qualifier_mask(mulib_img_rgba c,float lo,float hi,float satlo,float sathi,float soft){float max=fmaxf(c.r,fmaxf(c.g,c.b)),min=fminf(c.r,fminf(c.g,c.b));float sat=max>0?(max-min)/max:0;return mulib_color_qualifier_band(mulib_img_luma(c),lo,hi,soft)*mulib_color_qualifier_band(sat,satlo,sathi,soft);}
#ifdef __cplusplus
}
#endif
#endif
