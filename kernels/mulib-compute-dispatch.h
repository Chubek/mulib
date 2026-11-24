/** @file mulib-compute-dispatch.h
 * Validated dispatch to provider and ceil-division workgroup sizing.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Actual execution delegated.
 */
#ifndef MULIB_COMPUTE_DISPATCH_H
#define MULIB_COMPUTE_DISPATCH_H
#include "mulib-compute-core.h"
#include "mulib-compute-pipeline.h"
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
/* fence is committed only on success; zero is reserved as invalid. Provider
 * must not retain the borrowed handles array after dispatch returns. */
static inline mulib_media_status mulib_compute_dispatch(mulib_compute_context *c,mulib_compute_pipeline p,mulib_compute_grid groups,const mulib_compute_handle *handles,size_t n,uint64_t *fence){
 if(!mulib_compute_context_valid(c)||!fence||!mulib_compute_grid_valid(groups,c->max_groups)||!mulib_compute_pipeline_bindings(p,handles,n))return MULIB_MEDIA_INVALID;
 for(size_t i=0;i<n;i++)if(handles[i].bytes>c->max_buffer)return MULIB_MEDIA_RANGE;
 uint64_t f=0;mulib_media_status s=c->dispatch(c->ctx,p.id,groups,handles,n,&f);
 if(s>MULIB_MEDIA_OK||s<MULIB_MEDIA_CANCELLED)return MULIB_MEDIA_IO;
 if(s==MULIB_MEDIA_OK){if(!f)return MULIB_MEDIA_IO;*fence=f;}return s;
}
static inline bool mulib_compute_dispatch_ceil(uint32_t count,uint32_t local,uint32_t *groups){if(!count||!local||!groups)return false;*groups=count/local+(count%local!=0);return true;}
#ifdef __cplusplus
}
#endif
#endif
