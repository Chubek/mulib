/* MuLIB browser staging addition: urlsearchparams
 * Implemented subset: Form URL encoding and percent/plus decoding of individual components.
 * Remaining: Ordered duplicate tuple list, parsing, sort, UTF-8 replacement.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_URLSEARCHPARAMS_H
#define MULIB_URLSEARCHPARAMS_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_urlsearchparams_encode(muba_span s,muba_buf *out){if(!out||!muba_span_ok(s))return 0;static const char hex[]="0123456789ABCDEF";muba_buf b={0};for(size_t i=0;i<s.len;i++){unsigned char c=s.data[i];if(c==' ')c='+';else if(!muba_alpha(c)&&!muba_digit(c)&&c!='*'&&c!='-'&&c!='.'&&c!='_'){unsigned char v[3]={'%',(unsigned char)hex[c>>4],(unsigned char)hex[c&15]};if(!muba_append(&b,(muba_span){v,3}))goto fail;continue;}if(!muba_byte(&b,c))goto fail;}return muba_commit(out,&b);fail:muba_free(&b);return 0;}
static inline int mulib_urlsearchparams_decode(muba_span s,muba_buf *out){if(!out||!muba_span_ok(s))return 0;muba_buf b={0};for(size_t i=0;i<s.len;i++){unsigned char c=s.data[i];if(c=='+')c=' ';else if(c=='%'&&s.len-i>=3){int a=muba_hex(s.data[i+1]),d=muba_hex(s.data[i+2]);if(a>=0&&d>=0){c=(unsigned char)(a*16+d);i+=2;}}if(!muba_byte(&b,c)){muba_free(&b);return 0;}}return muba_commit(out,&b);}
#endif /* MULIB_URLSEARCHPARAMS_H */
