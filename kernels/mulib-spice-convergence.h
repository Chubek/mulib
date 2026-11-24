/* MuLIB: spice-convergence. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_CONVERGENCE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_CONVERGENCE_H
#define MULIB_SPICE_CONVERGENCE_H
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
#include "mulib-num-core.h"
#include "mulib-geom2-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_spice_convergence_test(const double *x,const double *prior,size_t n,double atol,double rtol);
double mulib_spice_convergence_gmin(double initial,double final,size_t step,size_t steps);
double mulib_spice_convergence_limit(double voltage,double previous,double max_delta);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_CONVERGENCE) && !defined(MULIB_SPICE_CONVERGENCE_H_IMPLEMENTED)
#define MULIB_SPICE_CONVERGENCE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_convergence_test(const double *x,const double *prior,size_t n,double atol,double rtol){if(!mulib_num_all_finite(x,n)||!mulib_num_all_finite(prior,n)||atol<=0||rtol<0)return 0;for(size_t i=0;i<n;i++)if(fabs(x[i]-prior[i])>atol+rtol*MULIB_STORAGE_MAX(fabs(x[i]),fabs(prior[i])))return 0;return 1;}
double mulib_spice_convergence_gmin(double initial,double final,size_t step,size_t steps){if(initial<=0||final<=0||final>initial||!steps||step>steps)return NAN;return exp(log(initial)+(log(final)-log(initial))*((double)step/steps));}
double mulib_spice_convergence_limit(double voltage,double previous,double max_delta){if(max_delta<=0||!isfinite(voltage+previous+max_delta))return NAN;return previous+MULIB_STORAGE_MAX(-max_delta,MULIB_STORAGE_MIN(max_delta,voltage-previous));}
#ifdef __cplusplus
}
#endif
#endif
