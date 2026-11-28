/* MuLIB browser staging addition: web-imagebitmap
 * Implemented subset: Checked RGBA8 image size and bounded pixel copy.
 * Remaining: Image decoding, color conversion, alpha and origin taint.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_IMAGEBITMAP_H
#define MULIB_WEB_IMAGEBITMAP_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_web_imagebitmap_copy(size_t w,size_t h,muba_span pixels,size_t limit,muba_buf *out){size_t n;if(!out||!muba_mul_size(w,h,&n)||!muba_mul_size(n,4,&n)||n>limit||pixels.len!=n)return 0;muba_buf b={0};if(!muba_append(&b,pixels))return 0;return muba_commit(out,&b);}
#endif /* MULIB_WEB_IMAGEBITMAP_H */
