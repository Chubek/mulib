/** @file mulib-compute-sync.h
 * Fence provider wait and monotonic software timeline accounting.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Caller serializes operations; no native barriers.
 */
#ifndef MULIB_COMPUTE_SYNC_H
#define MULIB_COMPUTE_SYNC_H
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
static inline mulib_media_status mulib_compute_sync_wait(mulib_compute_context *c,uint64_t fence,uint64_t timeout_ns){if(!mulib_compute_context_valid(c)||!fence)return MULIB_MEDIA_INVALID;return c->wait(c->ctx,fence,timeout_ns);}
typedef struct {uint64_t submitted,completed;} mulib_compute_sync_timeline;
static inline bool mulib_compute_sync_submit(mulib_compute_sync_timeline *t,uint64_t *f){if(!t||!f||t->submitted==UINT64_MAX||t->completed>t->submitted)return false;*f=++t->submitted;return true;}
static inline bool mulib_compute_sync_complete(mulib_compute_sync_timeline *t,uint64_t f){if(!t||f<t->completed||f>t->submitted)return false;t->completed=f;return true;}
#ifdef __cplusplus
}
#endif
#endif
