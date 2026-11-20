/* MuLIB: hwil-reset. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_RESET in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_RESET_H
#define MULIB_HWIL_RESET_H
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

uint64_t mulib_hwil_reset_apply(uint64_t prior,uint64_t data,uint64_t reset_value,unsigned width,int clock_edge,int reset_asserted,int asynchronous);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_RESET) && !defined(MULIB_HWIL_RESET_H_IMPLEMENTED)
#define MULIB_HWIL_RESET_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
uint64_t mulib_hwil_reset_apply(uint64_t prior,uint64_t data,uint64_t reset_value,unsigned width,int clock_edge,int reset_asserted,int asynchronous){uint64_t mask=mulib_hwil_hw_mask(width);return (reset_asserted&&(asynchronous||clock_edge)?reset_value:clock_edge?data:prior)&mask;}
#ifdef __cplusplus
}
#endif
#endif
