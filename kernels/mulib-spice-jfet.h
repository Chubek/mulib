/* MuLIB: spice-jfet. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_JFET in exactly one implementation translation unit.
 * Profile: Static channel conduction model; not a complete JFET junction model.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_JFET_H
#define MULIB_SPICE_JFET_H
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
#include "mulib-spice-mosfet.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_spice_jfet_eval(double vgs,double vds,double pinch,double beta,double lambda,double *id,double *gm,double *gds);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_JFET) && !defined(MULIB_SPICE_JFET_H_IMPLEMENTED)
#define MULIB_SPICE_JFET_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_jfet_eval(double vgs,double vds,double pinch,double beta,double lambda,double *id,double *gm,double *gds){if(pinch>=0)return 0;return mulib_spice_mosfet_eval(vgs,vds,pinch,2*beta,lambda,id,gm,gds);}
#ifdef __cplusplus
}
#endif
#endif
