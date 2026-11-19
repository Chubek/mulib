/* MuLIB / Amb3r: num-adaptive. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_ADAPTIVE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_ADAPTIVE_H
#define MULIB_NUM_ADAPTIVE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

double mulib_num_adaptive_error(const double *approx,const double *reference,size_t n,double atol,double rtol);
double mulib_num_adaptive_step(double dt,double error,unsigned order,double minimum,double maximum);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_ADAPTIVE) && !defined(MULIB_NUM_ADAPTIVE_H_IMPLEMENTED)
#define MULIB_NUM_ADAPTIVE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
double mulib_num_adaptive_error(const double *approx,const double *reference,size_t n,double atol,double rtol){if(!approx||!reference||!n||atol<=0||rtol<0)return NAN;double e=0;for(size_t i=0;i<n;i++){double d=(approx[i]-reference[i])/(atol+rtol*MA_MAX(fabs(approx[i]),fabs(reference[i])));e=hypot(e,d);}return e/sqrt((double)n);}
double mulib_num_adaptive_step(double dt,double error,unsigned order,double minimum,double maximum){if(dt<=0||!isfinite(dt)||!isfinite(error)||error<0||!order||minimum<=0||maximum<minimum)return NAN;double factor=error==0?5:.9*pow(error,-1.0/(order+1.0));return MA_MAX(minimum,MA_MIN(maximum,dt*MA_MAX(.1,MA_MIN(5,factor))));}
#ifdef __cplusplus
}
#endif
#endif
