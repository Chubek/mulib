/* MuLIB / Amb3r: hdl-sdf. C11 single-header kernel.
 * Define MULIB_IMPL_HDL_SDF in exactly one implementation translation unit.
 * Profile: Min/typ/max triplet parse only.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HDL_SDF_H
#define MULIB_HDL_SDF_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_hdl_sdf_triplet(const char *s,double *min,double *typ,double *max);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HDL_SDF) && !defined(MULIB_HDL_SDF_H_IMPLEMENTED)
#define MULIB_HDL_SDF_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hdl_sdf_triplet(const char *s,double *min,double *typ,double *max){if(!s||!min||!typ||!max)return 0;char *p;double a=strtod(s,&p);if(p==s||*p!=':')return 0;const char *q=p+1;double b=strtod(q,&p);if(p==q||*p!=':')return 0;q=p+1;double c=strtod(q,&p);if(p==q||*p||!isfinite(a+b+c)||a>b||b>c)return 0;*min=a;*typ=b;*max=c;return 1;}
#ifdef __cplusplus
}
#endif
#endif
