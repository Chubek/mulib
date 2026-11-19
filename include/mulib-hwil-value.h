/* MuLIB / Amb3r: hwil-value. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_VALUE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_VALUE_H
#define MULIB_HWIL_VALUE_H
#include "mulib-amb3r-base.h"
#include "mulib-hwil-context.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t value,known,highz;unsigned width;} ma_hw_value;
int mulib_hwil_value_make(unsigned width,uint64_t value,uint64_t known,uint64_t highz,ma_hw_value *out);
ma_hw_value mulib_hwil_value_and(ma_hw_value a,ma_hw_value b);
ma_hw_value mulib_hwil_value_or(ma_hw_value a,ma_hw_value b);
ma_hw_value mulib_hwil_value_xor(ma_hw_value a,ma_hw_value b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_VALUE) && !defined(MULIB_HWIL_VALUE_H_IMPLEMENTED)
#define MULIB_HWIL_VALUE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_value_make(unsigned width,uint64_t value,uint64_t known,uint64_t highz,ma_hw_value *out){if(!out||!width||width>64||(known&highz))return 0;uint64_t mask=ma_hw_mask(width);*out=(ma_hw_value){value&known&mask,known&mask,highz&mask,width};return 1;}
ma_hw_value mulib_hwil_value_and(ma_hw_value a,ma_hw_value b){if(a.width!=b.width)return (ma_hw_value){0};uint64_t one=a.value&b.value&a.known&b.known,zero=(~a.value&a.known)|(~b.value&b.known);return (ma_hw_value){one,one|zero,0,a.width};}
ma_hw_value mulib_hwil_value_or(ma_hw_value a,ma_hw_value b){if(a.width!=b.width)return (ma_hw_value){0};uint64_t one=(a.value&a.known)|(b.value&b.known),zero=~a.value&~b.value&a.known&b.known;return (ma_hw_value){one,one|zero,0,a.width};}
ma_hw_value mulib_hwil_value_xor(ma_hw_value a,ma_hw_value b){if(a.width!=b.width)return (ma_hw_value){0};uint64_t k=a.known&b.known;return (ma_hw_value){(a.value^b.value)&k,k,0,a.width};}
#ifdef __cplusplus
}
#endif
#endif
