/* MuLIB browser staging addition: web-cachestorage
 * Implemented subset: Bounded exact URL entries; lookup matches only GET.
 * Remaining: Vary, ignore options, storage partitioning, response cloning and persistence.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_CACHESTORAGE_H
#define MULIB_WEB_CACHESTORAGE_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_span url;uintptr_t response;} mulib_web_cachestorage_entry;
static inline uintptr_t mulib_web_cachestorage_match(const mulib_web_cachestorage_entry *v,size_t n,muba_span method,muba_span url){if(!muba_eq(method,muba_str("GET")))return 0;if(v)for(size_t i=0;i<n;i++)if(muba_eq(v[i].url,url))return v[i].response;return 0;}

/* Start with len == 0. One caller-owned table per application namespace. */
typedef struct {
    mulib_web_cachestorage_entry *v;
    size_t len,cap;
} mulib_web_cachestorage_t;

static inline int mulib_web_cachestorage_valid_(const mulib_web_cachestorage_t *s,
    muba_span method, muba_span url)
{
    return s && s->len <= s->cap && (!s->cap || s->v) &&
           s->cap <= SIZE_MAX / sizeof *s->v && url.data && url.len &&
           muba_eq(method, muba_str("GET"));
}

/* Retain borrowed URL bytes for new entries; retain the original key on
 * overwrite. Response handles are nonzero and never owned by the table. */
static inline int mulib_web_cachestorage_put(mulib_web_cachestorage_t *s,
    muba_span method, muba_span url, uintptr_t response)
{
    if (!response || !mulib_web_cachestorage_valid_(s, method, url))
        return 0;
    for (size_t i = 0; i < s->len; i++) {
        if (muba_eq(s->v[i].url, url)) {
            s->v[i].response = response;
            return 1;
        }
    }
    if (s->len == s->cap)
        return 0;
    s->v[s->len].url = url;
    s->v[s->len++].response = response;
    return 1;
}

/* 1 for a removed entry, 0 for invalid arguments or a miss. */
static inline int mulib_web_cachestorage_delete(mulib_web_cachestorage_t *s,
    muba_span method, muba_span url)
{
    if (!mulib_web_cachestorage_valid_(s, method, url))
        return 0;
    for (size_t i = 0; i < s->len; i++) {
        if (muba_eq(s->v[i].url, url)) {
            if (i + 1 < s->len)
                memmove(s->v + i, s->v + i + 1, (s->len - i - 1) * sizeof *s->v);
            s->len--;
            return 1;
        }
    }
    return 0;
}
#endif /* MULIB_WEB_CACHESTORAGE_H */
