/* MuLIB: ecad-pin. C11 single-header kernel.
 * Define MULIB_IMPL_ECAD_PIN in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ECAD_PIN_H
#define MULIB_ECAD_PIN_H
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
#include "mulib-geom2-core.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {MULIB_ECAD_PIN_PASSIVE,MULIB_ECAD_PIN_IN,MULIB_ECAD_PIN_OUT,MULIB_ECAD_PIN_IO,MULIB_ECAD_PIN_POWER_IN,MULIB_ECAD_PIN_POWER_OUT};
typedef struct {uint64_t id,component;unsigned direction;mulib_geom2_point position;double length;} mulib_ecad_pin_record;
int mulib_ecad_pin_valid(const mulib_ecad_pin_record *p);
int mulib_ecad_pin_conflict(unsigned a,unsigned b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ECAD_PIN) && !defined(MULIB_ECAD_PIN_H_IMPLEMENTED)
#define MULIB_ECAD_PIN_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_pin_valid(const mulib_ecad_pin_record *p){return p&&p->id&&p->component&&p->direction<=MULIB_ECAD_PIN_POWER_OUT&&mulib_geom2_point_valid(p->position)&&isfinite(p->length)&&p->length>=0;}
int mulib_ecad_pin_conflict(unsigned a,unsigned b){if(a>MULIB_ECAD_PIN_POWER_OUT||b>MULIB_ECAD_PIN_POWER_OUT)return -1;return (a==MULIB_ECAD_PIN_OUT||a==MULIB_ECAD_PIN_POWER_OUT)&&(b==MULIB_ECAD_PIN_OUT||b==MULIB_ECAD_PIN_POWER_OUT);}
#ifdef __cplusplus
}
#endif
#endif
