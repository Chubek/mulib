/** @file mulib-media-seek.h
 * Binary timestamp seek followed by keyframe backtracking.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_MEDIA_SEEK_H
#define MULIB_MEDIA_SEEK_H
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
typedef struct {int64_t pts;uint64_t byte_offset;bool key;} mulib_media_seek_entry;
/* Sorted PTS entries; finds latest key at or before target. */
static inline bool mulib_media_seek_find(const mulib_media_seek_entry *e,size_t n,int64_t pts,size_t *index){if(!e||!index)return false;size_t lo=0,hi=n;while(lo<hi){size_t mid=lo+(hi-lo)/2;if(e[mid].pts<=pts)lo=mid+1;else hi=mid;}while(lo){--lo;if(e[lo].key){*index=lo;return true;}}return false;}
#ifdef __cplusplus
}
#endif
#endif
