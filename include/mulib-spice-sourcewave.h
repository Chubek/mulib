/* MuLIB / Amb3r: spice-sourcewave. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_SOURCEWAVE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_SOURCEWAVE_H
#define MULIB_SPICE_SOURCEWAVE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

double mulib_spice_sourcewave_pulse(double t,double low,double high,double delay,double rise,double fall,double width,double period);
double mulib_spice_sourcewave_sin(double t,double offset,double amp,double freq,double delay,double damping,double phase);
double mulib_spice_sourcewave_pwl(const double *time,const double *value,size_t n,double t);
double mulib_spice_sourcewave_exp(double t,double v1,double v2,double td1,double tau1,double td2,double tau2);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_SOURCEWAVE) && !defined(MULIB_SPICE_SOURCEWAVE_H_IMPLEMENTED)
#define MULIB_SPICE_SOURCEWAVE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
double mulib_spice_sourcewave_pulse(double t,double low,double high,double delay,double rise,double fall,double width,double period){if(!isfinite(t+low+high+delay+rise+fall+width+period)||delay<0||rise<0||fall<0||width<0||period<=0||rise+width+fall>period)return NAN;if(t<delay)return low;double x=fmod(t-delay,period);if(x<rise)return low+(high-low)*x/rise;x-=rise;if(x<width)return high;x-=width;if(x<fall)return high+(low-high)*x/fall;return low;}
double mulib_spice_sourcewave_sin(double t,double offset,double amp,double freq,double delay,double damping,double phase){if(freq<0||delay<0||damping<0||!isfinite(t+offset+amp+freq+delay+damping+phase))return NAN;double x=MA_MAX(0,t-delay);return offset+amp*exp(-damping*x)*sin(2*MA_PI*freq*x+phase);}
double mulib_spice_sourcewave_pwl(const double *time,const double *value,size_t n,double t){if(!n||!time||!value||!isfinite(t))return NAN;for(size_t i=0;i<n;i++)if(!isfinite(time[i])||!isfinite(value[i])||(i&&time[i]<=time[i-1]))return NAN;if(t<=time[0])return value[0];for(size_t i=1;i<n;i++)if(t<=time[i])return value[i-1]+(value[i]-value[i-1])*(t-time[i-1])/(time[i]-time[i-1]);return value[n-1];}
double mulib_spice_sourcewave_exp(double t,double v1,double v2,double td1,double tau1,double td2,double tau2){if(tau1<=0||tau2<=0||td1<0||td2<td1)return NAN;return v1+(t>td1?(v2-v1)*(-expm1(-(t-td1)/tau1)):0)+(t>td2?(v1-v2)*(-expm1(-(t-td2)/tau2)):0);}
#ifdef __cplusplus
}
#endif
#endif
