/** @file mulib-nle-track.h
 * Track lookup and mute/solo policy.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_NLE_TRACK_H
#define MULIB_NLE_TRACK_H
#include "mulib-nle-timeline.h"
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
typedef struct {uint64_t id;unsigned order;bool mute,solo,locked;float gain;} mulib_nle_track;
static inline bool mulib_nle_track_audible(const mulib_nle_track *t,size_t n,size_t index){if(!t||index>=n||t[index].mute)return false;bool solo=false;for(size_t i=0;i<n;i++)solo|=t[i].solo&&!t[i].mute;return !solo||t[index].solo;}
static inline size_t mulib_nle_track_find(const mulib_nle_track *t,size_t n,uint64_t id){if(t)for(size_t i=0;i<n;i++)if(t[i].id==id)return i;return SIZE_MAX;}
#ifdef __cplusplus
}
#endif
#endif
