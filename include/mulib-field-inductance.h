/* MuLIB / Amb3r: field-inductance. C11 single-header kernel.
 * Define MULIB_IMPL_FIELD_INDUCTANCE in exactly one implementation translation unit.
 * Profile: Energy-derived and ideal solenoid formulas, not general geometry extraction.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FIELD_INDUCTANCE_H
#define MULIB_FIELD_INDUCTANCE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

double mulib_field_inductance_energy(double energy,double current);
double mulib_field_inductance_solenoid(double mu,double turns,double area,double length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FIELD_INDUCTANCE) && !defined(MULIB_FIELD_INDUCTANCE_H_IMPLEMENTED)
#define MULIB_FIELD_INDUCTANCE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
double mulib_field_inductance_energy(double energy,double current){if(energy<0||!isfinite(energy)||!isfinite(current)||current==0)return NAN;return 2*energy/(current*current);}
double mulib_field_inductance_solenoid(double mu,double turns,double area,double length){if(mu<=0||turns<=0||area<=0||length<=0)return NAN;return mu*turns*turns*area/length;}
#ifdef __cplusplus
}
#endif
#endif
