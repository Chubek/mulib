/* MuLIB: spice-mosfet. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_MOSFET in exactly one implementation translation unit.
 * Profile: Static long-channel level-1 NMOS model; no body effect, capacitance, BSIM or noise.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_MOSFET_H
#define MULIB_SPICE_MOSFET_H
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

int mulib_spice_mosfet_eval(double vgs,double vds,double vt,double beta,double lambda,double *id,double *gm,double *gds);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_MOSFET) && !defined(MULIB_SPICE_MOSFET_H_IMPLEMENTED)
#define MULIB_SPICE_MOSFET_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_mosfet_eval(double vgs,double vds,double vt,double beta,double lambda,double *id,double *gm,double *gds){if(!id||!gm||!gds||beta<0||lambda<0||!isfinite(vgs+vds+vt+beta+lambda))return 0;int reverse=vds<0;if(reverse){vgs-=vds;vds=-vds;}double ov=vgs-vt,i=0,g=0,d=0;if(ov>0){double factor=1+lambda*vds;if(vds<ov){double q=beta*(ov*vds-.5*vds*vds);i=q*factor;g=beta*vds*factor;d=beta*(ov-vds)*factor+q*lambda;}else{double q=.5*beta*ov*ov;i=q*factor;g=beta*ov*factor;d=q*lambda;}}*id=reverse?-i:i;*gm=reverse?-g:g;*gds=reverse?d+g:d;return isfinite(*id)&&isfinite(*gm)&&isfinite(*gds);}
#ifdef __cplusplus
}
#endif
#endif
