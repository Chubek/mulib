/** @file mulib-compute-core.h
 * Device-neutral dispatch and fence contract.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_COMPUTE_CORE_H
#define MULIB_COMPUTE_CORE_H
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
typedef enum {MULIB_COMPUTE_CPU,MULIB_COMPUTE_VULKAN,MULIB_COMPUTE_METAL,MULIB_COMPUTE_D3D12,MULIB_COMPUTE_WEBGPU} mulib_compute_backend;
typedef struct {uint32_t x,y,z;} mulib_compute_grid;
typedef struct {uint64_t id;size_t bytes;} mulib_compute_handle;
typedef mulib_media_status (*mulib_compute_dispatch_fn)(void*,uint64_t,mulib_compute_grid,const mulib_compute_handle*,size_t,uint64_t*);
typedef struct {mulib_compute_backend backend;void *ctx;mulib_compute_grid max_groups;size_t max_buffer;mulib_compute_dispatch_fn dispatch;mulib_media_status (*wait)(void*,uint64_t,uint64_t);void (*close)(void*);} mulib_compute_context;
/* Providers own native validation: pipeline/device identity, handle generations,
 * buffer access/aliasing, local-size limits, synchronization and actual device
 * submission. These adapters cannot prove that a provider uses a GPU. Contexts
 * are move-only, caller-serialized, and must be empty before attachment. */
static inline bool mulib_compute_backend_valid(mulib_compute_backend b){return b>=MULIB_COMPUTE_CPU&&b<=MULIB_COMPUTE_WEBGPU;}
static inline bool mulib_compute_context_valid(const mulib_compute_context *c){return c&&mulib_compute_backend_valid(c->backend)&&c->ctx&&c->dispatch&&c->wait&&c->close&&c->max_buffer&&c->max_groups.x&&c->max_groups.y&&c->max_groups.z;}
static inline bool mulib_compute_grid_valid(mulib_compute_grid a,mulib_compute_grid max){return a.x&&a.y&&a.z&&a.x<=max.x&&a.y<=max.y&&a.z<=max.z;}
static inline void mulib_compute_context_close(mulib_compute_context *c){if(c){if(c->close)c->close(c->ctx);memset(c,0,sizeof *c);}}
#ifdef __cplusplus
}
#endif
#endif
