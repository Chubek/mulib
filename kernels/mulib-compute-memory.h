/** @file mulib-compute-memory.h
 * Aligned suballocation offsets for a preallocated staging arena.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Not device memory allocation; fence before reuse.
 */
#ifndef MULIB_COMPUTE_MEMORY_H
#define MULIB_COMPUTE_MEMORY_H
#include "mulib-compute-core.h"
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
typedef struct {size_t capacity,cursor;} mulib_compute_memory_arena;
static inline bool mulib_compute_memory_alloc(mulib_compute_memory_arena *a,size_t n,size_t align,size_t *off){if(!a||!off||!n||!align||(align&(align-1))||a->cursor>a->capacity||a->cursor>SIZE_MAX-(align-1))return false;size_t p=(a->cursor+align-1)&~(align-1);if(p>a->capacity||n>a->capacity-p)return false;*off=p;a->cursor=p+n;return true;}
static inline bool mulib_compute_memory_reset(mulib_compute_memory_arena *a,size_t marker){if(!a||a->cursor>a->capacity||marker>a->cursor)return false;a->cursor=marker;return true;}
#ifdef __cplusplus
}
#endif
#endif
