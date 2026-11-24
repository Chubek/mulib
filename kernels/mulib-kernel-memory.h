/** @file mulib-kernel-memory.h
 * Overflow-safe interval bounds and alias checks for allocation IDs.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No pointer provenance inference or address-space lowering.
 */
#ifndef MULIB_KERNEL_MEMORY_H
#define MULIB_KERNEL_MEMORY_H
#include "mulib-kernel-ir.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t allocation;size_t offset,length;} mulib_kernel_memory_region;
static inline bool mulib_kernel_memory_valid(mulib_kernel_memory_region r,size_t capacity){return r.allocation&&r.offset<=capacity&&r.length<=capacity-r.offset;}
static inline bool mulib_kernel_memory_alias(mulib_kernel_memory_region a,mulib_kernel_memory_region b){if(!a.allocation||a.allocation!=b.allocation||!a.length||!b.length)return false;return a.offset<=b.offset?b.offset-a.offset<a.length:a.offset-b.offset<b.length;}
#ifdef __cplusplus
}
#endif
#endif
