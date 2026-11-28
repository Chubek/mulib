/* MuLIB browser staging addition: web-encoding
 * Implemented subset: UTF-8 validation/replacement and Windows-1252 decoding.
 * Remaining: Streaming state, UTF-16, other legacy encoding tables and exact error consumption.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_ENCODING_H
#define MULIB_WEB_ENCODING_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_web_encoding_decode(muba_span s,int cp1252,muba_buf *out){if(!out||!muba_span_ok(s))return 0;static const uint16_t map[32]={0x20ac,0x81,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,0x2c6,0x2030,0x160,0x2039,0x152,0x8d,0x17d,0x8f,0x90,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0x9d,0x17e,0x178};muba_buf b={0};size_t i=0;while(i<s.len){uint32_t c;if(cp1252){c=s.data[i++];if(c>=128&&c<160)c=map[c-128];}else if(!muba_utf8_next(s,&i,&c,0)){i++;c=0xfffd;}if(!muba_utf8_put(&b,c,0)){muba_free(&b);return 0;}}return muba_commit(out,&b);}
#endif /* MULIB_WEB_ENCODING_H */
