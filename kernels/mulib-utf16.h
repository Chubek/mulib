/* MuLIB browser staging addition: utf16
 * Implemented subset: UTF-8/WTF-8 decoding to native-endian UTF-16 and UTF-16 encoding.
 * Remaining: Streaming, endian-specific formats and exhaustive encoding conformance.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_UTF16_H
#define MULIB_UTF16_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_utf16_decode(muba_span s,int surrogate,muba_buf *out){if(!out||!muba_span_ok(s))return 0;muba_buf b={0};size_t i=0;while(i<s.len){uint32_t c;if(!muba_utf8_next(s,&i,&c,surrogate))goto fail;uint16_t w[2];size_t n=1;if(c>65535){c-=65536;w[0]=(uint16_t)(0xd800+(c>>10));w[1]=(uint16_t)(0xdc00+(c&1023));n=2;}else w[0]=(uint16_t)c;if(!muba_append(&b,(muba_span){(const unsigned char*)w,n*2}))goto fail;}return muba_commit(out,&b);fail:muba_free(&b);return 0;}
static inline int mulib_utf16_encode(const uint16_t *s,size_t n,int surrogate,muba_buf *out){if(!out||(!s&&n))return 0;muba_buf b={0};for(size_t i=0;i<n;i++){uint32_t c=s[i];if(c>=0xd800&&c<=0xdbff&&i+1<n&&s[i+1]>=0xdc00&&s[i+1]<=0xdfff){c=0x10000+((c-0xd800)<<10)+(s[++i]-0xdc00);}else if(c>=0xd800&&c<=0xdfff&&!surrogate)c=0xfffd;if(!muba_utf8_put(&b,c,surrogate)){muba_free(&b);return 0;}}return muba_commit(out,&b);}
#endif /* MULIB_UTF16_H */
