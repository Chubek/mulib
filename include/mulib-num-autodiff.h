/* MuLIB / Amb3r: num-autodiff. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_AUTODIFF in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_AUTODIFF_H
#define MULIB_NUM_AUTODIFF_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {double v,d;} ma_dual;
ma_dual mulib_num_autodiff_add(ma_dual a,ma_dual b);
ma_dual mulib_num_autodiff_mul(ma_dual a,ma_dual b);
ma_dual mulib_num_autodiff_div(ma_dual a,ma_dual b);
ma_dual mulib_num_autodiff_exp(ma_dual a);
ma_dual mulib_num_autodiff_log(ma_dual a);
ma_dual mulib_num_autodiff_sin(ma_dual a);
ma_dual mulib_num_autodiff_sqrt(ma_dual a);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_AUTODIFF) && !defined(MULIB_NUM_AUTODIFF_H_IMPLEMENTED)
#define MULIB_NUM_AUTODIFF_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
ma_dual mulib_num_autodiff_add(ma_dual a,ma_dual b){return (ma_dual){a.v+b.v,a.d+b.d};}
ma_dual mulib_num_autodiff_mul(ma_dual a,ma_dual b){return (ma_dual){a.v*b.v,a.v*b.d+a.d*b.v};}
ma_dual mulib_num_autodiff_div(ma_dual a,ma_dual b){return (ma_dual){a.v/b.v,(a.d*b.v-a.v*b.d)/(b.v*b.v)};}
ma_dual mulib_num_autodiff_exp(ma_dual a){double v=exp(a.v);return (ma_dual){v,v*a.d};}
ma_dual mulib_num_autodiff_log(ma_dual a){return (ma_dual){log(a.v),a.d/a.v};}
ma_dual mulib_num_autodiff_sin(ma_dual a){return (ma_dual){sin(a.v),cos(a.v)*a.d};}
ma_dual mulib_num_autodiff_sqrt(ma_dual a){double s=sqrt(a.v);return (ma_dual){s,a.d/(2*s)};}
#ifdef __cplusplus
}
#endif
#endif
