/* MuLIB browser staging addition: web-textcodec
 * Implemented subset: UTF-16 to UTF-8 USVString encoding and UTF-8 replacement decoding.
 * Remaining: Streaming decoder, fatal and ignoreBOM modes, all labels.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_TEXTCODEC_H
#define MULIB_WEB_TEXTCODEC_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-utf16.h"
#include "mulib-web-encoding.h"
static inline int mulib_web_textcodec_encode(const uint16_t *s,size_t n,muba_buf *out){return mulib_utf16_encode(s,n,0,out);}
static inline int mulib_web_textcodec_decode(muba_span s,muba_buf *out){return mulib_web_encoding_decode(s,0,out);}
#endif /* MULIB_WEB_TEXTCODEC_H */
