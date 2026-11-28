/* MuLIB browser staging addition: wtf8
 * Implemented subset: Lossless UTF-16/WTF-8 conversions.
 * Remaining: Canonical WTF-8 validation and integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WTF8_H
#define MULIB_WTF8_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-utf16.h"
static inline int mulib_wtf8_from_utf16(const uint16_t *s,size_t n,muba_buf *out){return mulib_utf16_encode(s,n,1,out);}
static inline int mulib_wtf8_to_utf16(muba_span s,muba_buf *out){return mulib_utf16_decode(s,1,out);}
#endif /* MULIB_WTF8_H */
