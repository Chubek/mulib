/** @file mulib-asset-core.h
 * Stable 128-bit identity and borrowed asset metadata.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_ASSET_CORE_H
#define MULIB_ASSET_CORE_H
#include "mulib-media-core.h"
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
typedef struct {uint64_t hi,lo;} mulib_asset_id;
typedef struct {mulib_asset_id id;uint64_t revision;const char *path,*label;uint32_t kind;} mulib_asset_record;
static inline bool mulib_asset_id_equal(mulib_asset_id a,mulib_asset_id b){return a.hi==b.hi&&a.lo==b.lo;}
static inline bool mulib_asset_id_valid(mulib_asset_id a){return a.hi||a.lo;}
static inline bool mulib_asset_record_valid(mulib_asset_record a){return mulib_asset_id_valid(a.id)&&a.path&&*a.path&&a.label&&a.kind;}
#ifdef __cplusplus
}
#endif
#endif
