/* MuLIB browser staging addition: web-compressionstream
 * Implemented subset: Algorithm name selection for provider-backed compression.
 * Remaining: Actual streaming compression/decompression, validation and backpressure.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_COMPRESSIONSTREAM_H
#define MULIB_WEB_COMPRESSIONSTREAM_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_web_compressionstream_algorithm(muba_span s){return muba_eq(s,muba_str("gzip"))?1:muba_eq(s,muba_str("deflate"))?2:muba_eq(s,muba_str("deflate-raw"))?3:0;}
#endif /* MULIB_WEB_COMPRESSIONSTREAM_H */
