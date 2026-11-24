/** @file mulib-img-keying.h
 * Soft chroma-distance alpha key.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No spill suppression or foreground color recovery.
 */
#ifndef MULIB_IMG_KEYING_H
#define MULIB_IMG_KEYING_H
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
static inline bool mulib_img_keying(mulib_img_view d,mulib_img_view s,float kr,float kg,float kb,float near,float far){if(!mulib_img_same(d,s)||!mulib_img_pointwise_alias(d,s)||!isfinite(kr)||!isfinite(kg)||!isfinite(kb)||!isfinite(near)||!isfinite(far)||near<0||far<=near)return false;double ku=kb-((double)kr+kg+kb)/3,kv=kr-((double)kr+kg+kb)/3;for(size_t y=0;y<s.height;y++)for(size_t x=0;x<s.width;x++){mulib_img_rgba c=mulib_img_get(s,x,y);double l=((double)c.r+c.g+c.b)/3;float z=(float)fmax(0,fmin((hypot(c.b-l-ku,c.r-l-kv)-near)/((double)far-near),1));c.a*=z*z*(3-2*z);mulib_img_put(d,x,y,c);}return true;}
#ifdef __cplusplus
}
#endif
#endif
