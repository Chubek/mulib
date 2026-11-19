/* MuLIB / Amb3r: spice-pz. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_PZ in exactly one implementation translation unit.
 * Profile: First- and second-order root extraction only.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_PZ_H
#define MULIB_SPICE_PZ_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_spice_pz_roots(double a,double b,double c,ma_complex *roots,size_t *n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_PZ) && !defined(MULIB_SPICE_PZ_H_IMPLEMENTED)
#define MULIB_SPICE_PZ_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_pz_roots(double a,double b,double c,ma_complex *roots,size_t *n){if(!roots||!n||!isfinite(a+b+c))return 0;if(a==0){if(b==0){*n=0;return c!=0;}roots[0]=(ma_complex){-c/b,0};*n=1;return 1;}double d=b*b-4*a*c;if(d>=0){double q=-.5*(b+copysign(sqrt(d),b));roots[0]=(ma_complex){q/a,0};roots[1]=(ma_complex){q?c/q:0,0};}else{roots[0]=(ma_complex){-b/(2*a),sqrt(-d)/(2*fabs(a))};roots[1]=(ma_complex){roots[0].re,-roots[0].im};}*n=2;return 1;}
#ifdef __cplusplus
}
#endif
#endif
