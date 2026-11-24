/* MuLIB: num-bdf. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_BDF in exactly one implementation translation unit.
 * Profile: Constant-step BDF coefficients and differentiation; not a variable-order stateful integrator.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_BDF_H
#define MULIB_NUM_BDF_H
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
#include "mulib-num-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_num_bdf_coefficients(unsigned order,double *a,size_t capacity);
int mulib_num_bdf_derivative(unsigned order,size_t n,const double *history,double dt,double *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_BDF) && !defined(MULIB_NUM_BDF_H_IMPLEMENTED)
#define MULIB_NUM_BDF_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_bdf_coefficients(unsigned order,double *a,size_t capacity){static const double table[6][7]={{1,-1},{1.5,-2,.5},{11.0/6,-3,1.5,-1.0/3},{25.0/12,-4,3,-4.0/3,.25},{137.0/60,-5,5,-10.0/3,1.25,-.2},{49.0/20,-6,7.5,-20.0/3,3.75,-1.2,1.0/6}};if(!order||order>6||!a||capacity<order+1)return 0;memcpy(a,table[order-1],(order+1)*sizeof(double));return 1;}
int mulib_num_bdf_derivative(unsigned order,size_t n,const double *history,double dt,double *out){double a[7];if(!history||!out||dt<=0||!isfinite(dt)||!mulib_num_bdf_coefficients(order,a,7))return 0;for(size_t i=0;i<n;i++){double s=0;for(unsigned k=0;k<=order;k++)s+=a[k]*history[(size_t)k*n+i];out[i]=s/dt;}return mulib_num_all_finite(out,n);}
#ifdef __cplusplus
}
#endif
#endif
