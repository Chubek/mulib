/* MuLIB browser staging addition: locale-number
 * Implemented subset: Decimal integer grouping with caller-supplied separator.
 * Remaining: CLDR formatting, rounding, numbering systems and units.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_LOCALE_NUMBER_H
#define MULIB_LOCALE_NUMBER_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_locale_number_integer(int64_t v,unsigned char sep,muba_buf *out){if(!out)return 0;char s[32];int n=snprintf(s,sizeof s,"%lld",(long long)v);if(n<0||(size_t)n>=sizeof s)return 0;muba_buf b={0};size_t i=0;if(s[0]=='-'){if(!muba_byte(&b,'-'))goto fail;i=1;}for(;i<(size_t)n;i++){if(i&&s[i-1]!='-'&&((size_t)n-i)%3==0&&!muba_byte(&b,sep))goto fail;if(!muba_byte(&b,(unsigned char)s[i]))goto fail;}return muba_commit(out,&b);fail:muba_free(&b);return 0;}
#endif /* MULIB_LOCALE_NUMBER_H */
