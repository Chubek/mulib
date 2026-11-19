/* MuLIB / Amb3r: field-capacitance. C11 single-header kernel.
 * Define MULIB_IMPL_FIELD_CAPACITANCE in exactly one implementation translation unit.
 * Profile: Energy-derived and parallel-plate capacitance formulas, not a geometry extractor.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FIELD_CAPACITANCE_H
#define MULIB_FIELD_CAPACITANCE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

double mulib_field_capacitance_energy(double energy,double voltage);
double mulib_field_capacitance_parallel(double area,double separation,double epsilon);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FIELD_CAPACITANCE) && !defined(MULIB_FIELD_CAPACITANCE_H_IMPLEMENTED)
#define MULIB_FIELD_CAPACITANCE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
double mulib_field_capacitance_energy(double energy,double voltage){if(energy<0||!isfinite(energy)||!isfinite(voltage)||voltage==0)return NAN;return 2*energy/(voltage*voltage);}
double mulib_field_capacitance_parallel(double area,double separation,double epsilon){if(area<=0||separation<=0||epsilon<=0)return NAN;return epsilon*area/separation;}
#ifdef __cplusplus
}
#endif
#endif
