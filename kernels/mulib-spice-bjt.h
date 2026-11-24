/* MuLIB: spice-bjt. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_BJT in exactly one implementation translation unit.
 * Profile: Static NPN Ebers-Moll model; no dynamic charge, Early effect or Gummel-Poon.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_BJT_H
#define MULIB_SPICE_BJT_H
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
#include "mulib-spice-diode.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_spice_bjt_eval(double vbe,double vbc,double is,double af,double ar,double temp,double *ic,double *ib,double *ie);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_BJT) && !defined(MULIB_SPICE_BJT_H_IMPLEMENTED)
#define MULIB_SPICE_BJT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_bjt_eval(double vbe,double vbc,double is,double af,double ar,double temp,double *ic,double *ib,double *ie){if(!ic||!ib||!ie||af<=0||af>=1||ar<=0||ar>=1)return 0;double f,r,d;if(!mulib_spice_diode_eval(vbe,is/af,1,temp,&f,&d)||!mulib_spice_diode_eval(vbc,is/ar,1,temp,&r,&d))return 0;*ic=af*f-r;*ie=-f+ar*r;*ib=-*ic-*ie;return isfinite(*ib)&&isfinite(*ic)&&isfinite(*ie);}
#ifdef __cplusplus
}
#endif
#endif
