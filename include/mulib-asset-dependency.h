/** @file mulib-asset-dependency.h
 * Transitive dependent-asset invalidation.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_ASSET_DEPENDENCY_H
#define MULIB_ASSET_DEPENDENCY_H
#include "mulib-asset-core.h"
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
typedef struct {size_t asset,dependency;} mulib_asset_dependency;
static inline bool mulib_asset_dependency_mark(const mulib_asset_dependency *e,size_t count,size_t n,bool *dirty){if((!e&&count)||(!dirty&&n))return false;for(size_t i=0;i<count;i++)if(e[i].asset>=n||e[i].dependency>=n)return false;for(size_t k=0;k<n;k++){bool change=false;for(size_t i=0;i<count;i++)if(dirty[e[i].dependency]&&!dirty[e[i].asset]){dirty[e[i].asset]=true;change=true;}if(!change)break;}return true;}
#ifdef __cplusplus
}
#endif
#endif
