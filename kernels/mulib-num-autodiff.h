/* MuLIB: num-autodiff. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_AUTODIFF in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_AUTODIFF_H
#define MULIB_NUM_AUTODIFF_H
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
typedef struct {double v,d;} mulib_num_dual;
mulib_num_dual mulib_num_autodiff_add(mulib_num_dual a,mulib_num_dual b);
mulib_num_dual mulib_num_autodiff_mul(mulib_num_dual a,mulib_num_dual b);
mulib_num_dual mulib_num_autodiff_div(mulib_num_dual a,mulib_num_dual b);
mulib_num_dual mulib_num_autodiff_exp(mulib_num_dual a);
mulib_num_dual mulib_num_autodiff_log(mulib_num_dual a);
mulib_num_dual mulib_num_autodiff_sin(mulib_num_dual a);
mulib_num_dual mulib_num_autodiff_sqrt(mulib_num_dual a);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_AUTODIFF) && !defined(MULIB_NUM_AUTODIFF_H_IMPLEMENTED)
#define MULIB_NUM_AUTODIFF_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
mulib_num_dual mulib_num_autodiff_add(mulib_num_dual a,mulib_num_dual b){return (mulib_num_dual){a.v+b.v,a.d+b.d};}
mulib_num_dual mulib_num_autodiff_mul(mulib_num_dual a,mulib_num_dual b){return (mulib_num_dual){a.v*b.v,a.v*b.d+a.d*b.v};}
mulib_num_dual mulib_num_autodiff_div(mulib_num_dual a,mulib_num_dual b){return (mulib_num_dual){a.v/b.v,(a.d*b.v-a.v*b.d)/(b.v*b.v)};}
mulib_num_dual mulib_num_autodiff_exp(mulib_num_dual a){double v=exp(a.v);return (mulib_num_dual){v,v*a.d};}
mulib_num_dual mulib_num_autodiff_log(mulib_num_dual a){return (mulib_num_dual){log(a.v),a.d/a.v};}
mulib_num_dual mulib_num_autodiff_sin(mulib_num_dual a){return (mulib_num_dual){sin(a.v),cos(a.v)*a.d};}
mulib_num_dual mulib_num_autodiff_sqrt(mulib_num_dual a){double s=sqrt(a.v);return (mulib_num_dual){s,a.d/(2*s)};}
#ifdef __cplusplus
}
#endif
#endif
