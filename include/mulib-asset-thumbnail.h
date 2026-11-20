/** @file mulib-asset-thumbnail.h
 * Aspect-preserving thumbnail from an RGBA8 frame.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Nearest-neighbor; no video seek or cache.
 */
#ifndef MULIB_ASSET_THUMBNAIL_H
#define MULIB_ASSET_THUMBNAIL_H
#include "mulib-media-proxy.h"
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
static inline bool mulib_asset_thumbnail(mulib_media_frame *out,const mulib_media_frame *src,size_t max_edge){return mulib_media_proxy_make(out,src,max_edge,max_edge);}
#ifdef __cplusplus
}
#endif
#endif
