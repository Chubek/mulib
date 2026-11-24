/* MuLIB: timing-parasitic. C11 single-header kernel.
 * Define MULIB_IMPL_TIMING_PARASITIC in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_TIMING_PARASITIC_H
#define MULIB_TIMING_PARASITIC_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_timing_parasitic_rc(double sheet_resistance,double length,double width,double capacitance_per_length,double fringe,double *r,double *c);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TIMING_PARASITIC) && !defined(MULIB_TIMING_PARASITIC_H_IMPLEMENTED)
#define MULIB_TIMING_PARASITIC_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_timing_parasitic_rc(double sheet_resistance,double length,double width,double capacitance_per_length,double fringe,double *r,double *c){if(!r||!c||sheet_resistance<0||length<0||width<=0||capacitance_per_length<0||fringe<0||!isfinite(sheet_resistance+length+width+capacitance_per_length+fringe))return 0;*r=sheet_resistance*length/width;*c=length*capacitance_per_length+fringe;return isfinite(*r)&&isfinite(*c);}
#ifdef __cplusplus
}
#endif
#endif
