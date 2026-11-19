/* MuLIB / Amb3r: spice-switch. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_SWITCH in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_SWITCH_H
#define MULIB_SPICE_SWITCH_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {int on;} ma_spice_switch;
int mulib_spice_switch_update(ma_spice_switch *s,double control,double threshold,double hysteresis,double ron,double roff,double *conductance);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_SWITCH) && !defined(MULIB_SPICE_SWITCH_H_IMPLEMENTED)
#define MULIB_SPICE_SWITCH_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_switch_update(ma_spice_switch *s,double control,double threshold,double hysteresis,double ron,double roff,double *conductance){if(!s||!conductance||hysteresis<0||ron<=0||roff<ron||!isfinite(control+threshold+hysteresis+ron+roff))return 0;if(control>threshold+hysteresis)s->on=1;else if(control<threshold-hysteresis)s->on=0;*conductance=1/(s->on?ron:roff);return 1;}
#ifdef __cplusplus
}
#endif
#endif
