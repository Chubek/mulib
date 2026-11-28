/* MuLIB browser staging addition: browser-protocolhandler
 * Implemented subset: Scheme validation and host callback dispatch by exact ASCII case-insensitive scheme.
 * Remaining: Custom protocol permissions, navigation integration and security.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_PROTOCOLHANDLER_H
#define MULIB_BROWSER_PROTOCOLHANDLER_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_browser_protocolhandler_fn)(void *,muba_span);typedef struct {muba_span scheme;mulib_browser_protocolhandler_fn fn;void *ctx;} mulib_browser_protocolhandler_entry;
static inline int mulib_browser_protocolhandler_dispatch(const mulib_browser_protocolhandler_entry *v,size_t n,muba_span url){if(!v||!muba_span_ok(url)||!url.len||!muba_alpha(url.data[0]))return 0;size_t i=1;while(i<url.len&&url.data[i]!=':'){unsigned char c=url.data[i++];if(!muba_alpha(c)&&!muba_digit(c)&&c!='+'&&c!='-'&&c!='.')return 0;}if(i==url.len)return 0;for(size_t j=0;j<n;j++)if(v[j].fn&&muba_ieq(v[j].scheme,(muba_span){url.data,i}))return v[j].fn(v[j].ctx,url);return 0;}
#endif /* MULIB_BROWSER_PROTOCOLHANDLER_H */
