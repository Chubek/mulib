/** @file mulib-compute-vulkan.h
 * Checked Vulkan queue provider attachment and dispatch.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Adapter only; no native vulkan device, command creation or driver code. Attach a real queue provider before advertising support.
 */
#ifndef MULIB_COMPUTE_VULKAN_H
#define MULIB_COMPUTE_VULKAN_H
#include "mulib-compute-core.h"
#include "mulib-compute-dispatch.h"
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
/* Attach a real, externally implemented Vulkan queue provider. Ownership moves on success. */
static inline bool mulib_compute_vulkan_attach(mulib_compute_context *out,mulib_compute_context *provider){if(!out||!provider||out==provider||out->ctx||out->dispatch||out->wait||out->close||provider->backend!=MULIB_COMPUTE_VULKAN||!provider->ctx||!provider->dispatch||!provider->wait||!provider->close||!provider->max_buffer||!provider->max_groups.x||!provider->max_groups.y||!provider->max_groups.z)return false;*out=*provider;memset(provider,0,sizeof *provider);return true;}
static inline mulib_media_status mulib_compute_vulkan_submit(mulib_compute_context *c,mulib_compute_pipeline p,mulib_compute_grid g,const mulib_compute_handle *h,size_t n,uint64_t *fence){if(!c||c->backend!=MULIB_COMPUTE_VULKAN)return MULIB_MEDIA_INVALID;return mulib_compute_dispatch(c,p,g,h,n,fence);}
#ifdef __cplusplus
}
#endif
#endif
