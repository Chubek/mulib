/** @file mulib-anim-easing.h
 * Six bounded easing functions.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_ANIM_EASING_H
#define MULIB_ANIM_EASING_H
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
typedef enum {MULIB_ANIM_EASE_LINEAR,MULIB_ANIM_EASE_IN,MULIB_ANIM_EASE_OUT,MULIB_ANIM_EASE_SMOOTH,MULIB_ANIM_EASE_SINE,MULIB_ANIM_EASE_QUINTIC} mulib_anim_easing;
static inline double mulib_anim_easing_eval(mulib_anim_easing m,double t){t=fmax(0,fmin(1,t));switch(m){case MULIB_ANIM_EASE_LINEAR:return t;case MULIB_ANIM_EASE_IN:return t*t;case MULIB_ANIM_EASE_OUT:return t*(2-t);case MULIB_ANIM_EASE_SMOOTH:return t*t*(3-2*t);case MULIB_ANIM_EASE_SINE:return .5-.5*cos(3.141592653589793*t);case MULIB_ANIM_EASE_QUINTIC:return t*t*t*(t*(6*t-15)+10);default:return NAN;}}
#ifdef __cplusplus
}
#endif
#endif
