/* MuLIB / Amb3r: ecad-pin. C11 single-header kernel.
 * Define MULIB_IMPL_ECAD_PIN in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ECAD_PIN_H
#define MULIB_ECAD_PIN_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {MA_PIN_PASSIVE,MA_PIN_IN,MA_PIN_OUT,MA_PIN_IO,MA_PIN_POWER_IN,MA_PIN_POWER_OUT};
typedef struct {uint64_t id,component;unsigned direction;ma_point position;double length;} ma_pin;
int mulib_ecad_pin_valid(const ma_pin *p);
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
int mulib_ecad_pin_valid(const ma_pin *p){return p&&p->id&&p->component&&p->direction<=MA_PIN_POWER_OUT&&ma_point_valid(p->position)&&isfinite(p->length)&&p->length>=0;}
int mulib_ecad_pin_conflict(unsigned a,unsigned b){if(a>MA_PIN_POWER_OUT||b>MA_PIN_POWER_OUT)return -1;return (a==MA_PIN_OUT||a==MA_PIN_POWER_OUT)&&(b==MA_PIN_OUT||b==MA_PIN_POWER_OUT);}
#ifdef __cplusplus
}
#endif
#endif
