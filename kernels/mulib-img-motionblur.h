/** @file mulib-img-motionblur.h
 * Velocity-map shutter integration.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No occlusion handling.
 */
#ifndef MULIB_IMG_MOTIONBLUR_H
#define MULIB_IMG_MOTIONBLUR_H
#include "mulib-img-core.h"
#include "mulib-img-sampler.h"
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
static inline bool mulib_img_motionblur(mulib_img_view d,mulib_img_view s,mulib_img_view velocity,unsigned samples){if(!mulib_img_same(d,s)||!mulib_img_same(d,velocity)||!mulib_img_disjoint(d,s)||!mulib_img_disjoint(d,velocity)||!samples||samples>128)return false;for(size_t y=0;y<d.height;y++)for(size_t x=0;x<d.width;x++){mulib_img_rgba v=mulib_img_get(velocity,x,y),c={0};for(unsigned i=0;i<samples;i++){double t=((double)i+.5)/samples-.5;mulib_img_rgba a=mulib_img_sample(s,x+t*v.r,y+t*v.g,MULIB_IMG_BILINEAR);c.r+=a.r;c.g+=a.g;c.b+=a.b;c.a+=a.a;}c.r/=samples;c.g/=samples;c.b/=samples;c.a/=samples;mulib_img_put(d,x,y,c);}return true;}
#ifdef __cplusplus
}
#endif
#endif
