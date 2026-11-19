/* MuLIB / Amb3r: hdl-liberty. C11 single-header kernel.
 * Define MULIB_IMPL_HDL_LIBERTY in exactly one implementation translation unit.
 * Profile: Bilinear interpolation for timing tables; not Liberty parsing.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HDL_LIBERTY_H
#define MULIB_HDL_LIBERTY_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_hdl_liberty_table(const double *x,size_t nx,const double *y,size_t ny,const double *values,double a,double b,double *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HDL_LIBERTY) && !defined(MULIB_HDL_LIBERTY_H_IMPLEMENTED)
#define MULIB_HDL_LIBERTY_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hdl_liberty_table(const double *x,size_t nx,const double *y,size_t ny,const double *values,double a,double b,double *out){if(!x||!y||!values||nx<2||ny<2||!out||!isfinite(a)||!isfinite(b)||a<x[0]||a>x[nx-1]||b<y[0]||b>y[ny-1])return 0;for(size_t i=1;i<nx;i++)if(x[i]<=x[i-1])return 0;for(size_t i=1;i<ny;i++)if(y[i]<=y[i-1])return 0;size_t i=0,j=0;while(i+2<nx&&a>x[i+1])i++;while(j+2<ny&&b>y[j+1])j++;double u=(a-x[i])/(x[i+1]-x[i]),v=(b-y[j])/(y[j+1]-y[j]);*out=(1-u)*((1-v)*values[i*ny+j]+v*values[i*ny+j+1])+u*((1-v)*values[(i+1)*ny+j]+v*values[(i+1)*ny+j+1]);return isfinite(*out);}
#ifdef __cplusplus
}
#endif
#endif
