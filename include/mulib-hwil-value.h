/* MuLIB: hwil-value. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_VALUE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_VALUE_H
#define MULIB_HWIL_VALUE_H
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
#include "mulib-hwil-context.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t value,known,highz;unsigned width;} mulib_hwil_hw_value;
int mulib_hwil_value_make(unsigned width,uint64_t value,uint64_t known,uint64_t highz,mulib_hwil_hw_value *out);
mulib_hwil_hw_value mulib_hwil_value_and(mulib_hwil_hw_value a,mulib_hwil_hw_value b);
mulib_hwil_hw_value mulib_hwil_value_or(mulib_hwil_hw_value a,mulib_hwil_hw_value b);
mulib_hwil_hw_value mulib_hwil_value_xor(mulib_hwil_hw_value a,mulib_hwil_hw_value b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_VALUE) && !defined(MULIB_HWIL_VALUE_H_IMPLEMENTED)
#define MULIB_HWIL_VALUE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_value_make(unsigned width,uint64_t value,uint64_t known,uint64_t highz,mulib_hwil_hw_value *out){if(!out||!width||width>64||(known&highz))return 0;uint64_t mask=mulib_hwil_hw_mask(width);*out=(mulib_hwil_hw_value){value&known&mask,known&mask,highz&mask,width};return 1;}
mulib_hwil_hw_value mulib_hwil_value_and(mulib_hwil_hw_value a,mulib_hwil_hw_value b){if(a.width!=b.width)return (mulib_hwil_hw_value){0};uint64_t one=a.value&b.value&a.known&b.known,zero=(~a.value&a.known)|(~b.value&b.known);return (mulib_hwil_hw_value){one,one|zero,0,a.width};}
mulib_hwil_hw_value mulib_hwil_value_or(mulib_hwil_hw_value a,mulib_hwil_hw_value b){if(a.width!=b.width)return (mulib_hwil_hw_value){0};uint64_t one=(a.value&a.known)|(b.value&b.known),zero=~a.value&~b.value&a.known&b.known;return (mulib_hwil_hw_value){one,one|zero,0,a.width};}
mulib_hwil_hw_value mulib_hwil_value_xor(mulib_hwil_hw_value a,mulib_hwil_hw_value b){if(a.width!=b.width)return (mulib_hwil_hw_value){0};uint64_t k=a.known&b.known;return (mulib_hwil_hw_value){(a.value^b.value)&k,k,0,a.width};}
#ifdef __cplusplus
}
#endif
#endif
