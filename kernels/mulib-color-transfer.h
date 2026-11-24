/** @file mulib-color-transfer.h
 * Linear/sRGB/Rec.709/PQ/HLG transfer functions.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: PQ uses normalized 10000-nit luminance; HLG is scene OETF, no OOTF.
 */
#ifndef MULIB_COLOR_TRANSFER_H
#define MULIB_COLOR_TRANSFER_H
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
typedef enum {MULIB_COLOR_TF_LINEAR,MULIB_COLOR_TF_SRGB,MULIB_COLOR_TF_REC709,MULIB_COLOR_TF_PQ,MULIB_COLOR_TF_HLG} mulib_color_tf;
static inline double mulib_color_transfer_decode(mulib_color_tf f,double x){switch(f){case MULIB_COLOR_TF_LINEAR:return x;case MULIB_COLOR_TF_SRGB:return x<=.04045?x/12.92:pow((x+.055)/1.055,2.4);case MULIB_COLOR_TF_REC709:return x<.081?x/4.5:pow((x+.099)/1.099,1/.45);case MULIB_COLOR_TF_PQ:{double p=pow(fmax(0,x),32.0/2523),d=2413.0/128-2392.0/128*p;return d>0?pow(fmax(0,p-3424.0/4096)/d,16384.0/2610):NAN;}case MULIB_COLOR_TF_HLG:return x<=.5?x*x/3:(exp((x-.55991073)/.17883277)+.28466892)/12;default:return NAN;}}
static inline double mulib_color_transfer_encode(mulib_color_tf f,double x){switch(f){case MULIB_COLOR_TF_LINEAR:return x;case MULIB_COLOR_TF_SRGB:return x<=.0031308?12.92*x:1.055*pow(x,1/2.4)-.055;case MULIB_COLOR_TF_REC709:return x<.018?4.5*x:1.099*pow(x,.45)-.099;case MULIB_COLOR_TF_PQ:{double p=pow(fmax(0,x),2610.0/16384);return pow((3424.0/4096+2413.0/128*p)/(1+2392.0/128*p),2523.0/32);}case MULIB_COLOR_TF_HLG:return x<=1.0/12?sqrt(fmax(0,3*x)):.17883277*log(12*x-.28466892)+.55991073;default:return NAN;}}
#ifdef __cplusplus
}
#endif
#endif
