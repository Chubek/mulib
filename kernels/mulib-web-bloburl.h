/* MuLIB browser staging addition: web-bloburl
 * Implemented subset: Origin-scoped opaque object registry lookup/revocation.
 * Remaining: URL generation, entropy, origin checks and lifetime.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_BLOBURL_H
#define MULIB_WEB_BLOBURL_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_span url;uint64_t origin;uintptr_t blob;} mulib_web_bloburl_entry;
static inline uintptr_t mulib_web_bloburl_get(const mulib_web_bloburl_entry *v,size_t n,uint64_t origin,muba_span url){if(v)for(size_t i=0;i<n;i++)if(v[i].origin==origin&&muba_eq(v[i].url,url))return v[i].blob;return 0;}
static inline int mulib_web_bloburl_revoke(mulib_web_bloburl_entry *v,size_t n,uint64_t origin,muba_span url){if(v)for(size_t i=0;i<n;i++)if(v[i].origin==origin&&v[i].blob&&muba_eq(v[i].url,url)){v[i].blob=0;return 1;}return 0;}
#endif /* MULIB_WEB_BLOBURL_H */
