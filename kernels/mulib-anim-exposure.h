/** @file mulib-anim-exposure.h
 * Midpoint stratified shutter-time samples.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_ANIM_EXPOSURE_H
#define MULIB_ANIM_EXPOSURE_H
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
static inline bool mulib_anim_exposure_samples(double center,double duration,double *out,size_t n){if(!out||!n||!isfinite(center)||!isfinite(duration)||duration<0||!isfinite(center+duration*.5)||!isfinite(center-duration*.5))return false;for(size_t i=0;i<n;i++)out[i]=center+duration*((i+.5)/n-.5);return true;}
#ifdef __cplusplus
}
#endif
#endif
