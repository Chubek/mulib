/** @file mulib-color-gamut.h
 * Neutral-axis gamut compression into bounded RGB.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Simple RGB heuristic, not perceptually uniform.
 */
#ifndef MULIB_COLOR_GAMUT_H
#define MULIB_COLOR_GAMUT_H
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
static inline mulib_img_rgba mulib_color_gamut_compress(mulib_img_rgba c,float ceiling){if(!isfinite(ceiling)||ceiling<=0)return c;float y=mulib_media_clampf(mulib_img_luma(c),0,ceiling),v[3]={c.r,c.g,c.b},k=1;for(int i=0;i<3;i++){float d=v[i]-y;if(d>0)k=fminf(k,(ceiling-y)/d);else if(d<0)k=fminf(k,-y/d);}c.r=y+k*(c.r-y);c.g=y+k*(c.g-y);c.b=y+k*(c.b-y);return c;}
#ifdef __cplusplus
}
#endif
#endif
