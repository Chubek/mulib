/* MuLIB / Amb3r: spice-diode. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_DIODE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_DIODE_H
#define MULIB_SPICE_DIODE_H
#include "mulib-amb3r-base.h"
#include "mulib-spice-mna.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_spice_diode_eval(double voltage,double saturation,double ideality,double temperature,double *current,double *derivative);
int mulib_spice_diode_stamp(ma_mna *m,size_t a,size_t b,double v,double is,double n,double t);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_DIODE) && !defined(MULIB_SPICE_DIODE_H_IMPLEMENTED)
#define MULIB_SPICE_DIODE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_diode_eval(double voltage,double saturation,double ideality,double temperature,double *current,double *derivative){if(!current||!derivative||saturation<=0||ideality<=0||temperature<=0||!isfinite(voltage+saturation+ideality+temperature))return 0;double vt=ideality*8.617333262145e-5*temperature,x=voltage/vt;if(x>40){double e=exp(40.0);*current=saturation*(e*(1+x-40)-1);*derivative=saturation*e/vt;}else{*current=saturation*expm1(x);*derivative=saturation*exp(x)/vt;}return isfinite(*current)&&isfinite(*derivative);}
int mulib_spice_diode_stamp(ma_mna *m,size_t a,size_t b,double v,double is,double n,double t){double i,g;if(!mulib_spice_diode_eval(v,is,n,t,&i,&g)||!ma_mna_nodes(m,a,b))return 0;return mulib_spice_mna_conductance(m,a,b,g)&&mulib_spice_mna_current(m,a,b,i-g*v);}
#ifdef __cplusplus
}
#endif
#endif
