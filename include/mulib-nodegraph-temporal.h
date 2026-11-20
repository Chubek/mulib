/** @file mulib-nodegraph-temporal.h
 * Checked temporal dependency window expansion.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_NODEGRAPH_TEMPORAL_H
#define MULIB_NODEGRAPH_TEMPORAL_H
#include "mulib-nodegraph-core.h"
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
typedef struct {int64_t first,last;} mulib_nodegraph_time_range;
static inline bool mulib_nodegraph_temporal_expand(mulib_nodegraph_time_range r,int64_t before,int64_t after,mulib_nodegraph_time_range *out){if(!out||r.first>r.last||before<0||after<0)return false;mulib_nodegraph_time_range t;if(!mulib_media_i64_add(r.first,-before,&t.first)||!mulib_media_i64_add(r.last,after,&t.last))return false;*out=t;return true;}
#ifdef __cplusplus
}
#endif
#endif
