/* MuLIB: field-material. C11 single-header kernel.
 * Define MULIB_IMPL_FIELD_MATERIAL in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FIELD_MATERIAL_H
#define MULIB_FIELD_MATERIAL_H
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
typedef struct {double epsilon,mu,conductivity,thermal_conductivity,density,heat_capacity;} mulib_field_material;
int mulib_field_material_valid(const mulib_field_material *m);
double mulib_field_material_diffusivity(const mulib_field_material *m);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FIELD_MATERIAL) && !defined(MULIB_FIELD_MATERIAL_H_IMPLEMENTED)
#define MULIB_FIELD_MATERIAL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_field_material_valid(const mulib_field_material *m){return m&&isfinite(m->epsilon)&&m->epsilon>0&&isfinite(m->mu)&&m->mu>0&&isfinite(m->conductivity)&&m->conductivity>=0&&isfinite(m->thermal_conductivity)&&m->thermal_conductivity>0&&isfinite(m->density)&&m->density>0&&isfinite(m->heat_capacity)&&m->heat_capacity>0;}
double mulib_field_material_diffusivity(const mulib_field_material *m){return mulib_field_material_valid(m)?m->thermal_conductivity/(m->density*m->heat_capacity):NAN;}
#ifdef __cplusplus
}
#endif
#endif
