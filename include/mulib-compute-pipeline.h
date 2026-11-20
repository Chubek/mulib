/** @file mulib-compute-pipeline.h
 * Pipeline binding layout validation.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Provider owns actual pipeline creation.
 */
#ifndef MULIB_COMPUTE_PIPELINE_H
#define MULIB_COMPUTE_PIPELINE_H
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
typedef struct {uint64_t id;mulib_compute_grid local;size_t binding_count;const size_t *min_bytes;} mulib_compute_pipeline;
/* min_bytes has binding_count size_t elements; h has n handle elements. The
 * provider must validate identity, actual capacity and native local-size limits. */
static inline bool mulib_compute_pipeline_bindings(mulib_compute_pipeline p,const mulib_compute_handle *h,size_t n){if(!p.id||!p.local.x||!p.local.y||!p.local.z||n!=p.binding_count||n>SIZE_MAX/sizeof *h||n>SIZE_MAX/sizeof *p.min_bytes||(n&&(!h||!p.min_bytes)))return false;for(size_t i=0;i<n;i++)if(!h[i].id||h[i].bytes<p.min_bytes[i])return false;return true;}
#ifdef __cplusplus
}
#endif
#endif
