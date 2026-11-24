/** @file mulib-compute-device.h
 * Select from real provider-supplied device capability descriptors.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No platform enumeration.
 */
#ifndef MULIB_COMPUTE_DEVICE_H
#define MULIB_COMPUTE_DEVICE_H
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
typedef struct {mulib_compute_backend backend;const char *name;size_t memory;unsigned score;bool available;} mulib_compute_device;
static inline size_t mulib_compute_device_select(const mulib_compute_device *d,size_t n,mulib_compute_backend preferred,size_t min_memory,bool fallback){if(!d||!mulib_compute_backend_valid(preferred)||n>SIZE_MAX/sizeof *d)return SIZE_MAX;size_t best=SIZE_MAX;for(size_t i=0;i<n;i++)if(mulib_compute_backend_valid(d[i].backend)&&d[i].available&&d[i].memory>=min_memory&&(fallback||d[i].backend==preferred)){if(best==SIZE_MAX||(d[i].backend==preferred&&d[best].backend!=preferred)||((d[i].backend==preferred)==(d[best].backend==preferred)&&d[i].score>d[best].score))best=i;}return best;}
#ifdef __cplusplus
}
#endif
#endif
