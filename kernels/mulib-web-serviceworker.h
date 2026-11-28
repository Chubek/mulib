/* MuLIB browser staging addition: web-serviceworker
 * Implemented subset: Longest scope prefix of a fragment-free serialized URL.
 * Remaining: Install/activate/update lifecycle, Fetch interception, scope authorization.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_SERVICEWORKER_H
#define MULIB_WEB_SERVICEWORKER_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {
    muba_span scope;
    uint64_t origin, worker;
} mulib_web_serviceworker_registration;

/* URLs and origin IDs must already be validated by the caller. */
static inline uint64_t mulib_web_serviceworker_match(
    const mulib_web_serviceworker_registration *v, size_t n, uint64_t origin,
    muba_span url)
{
    if (!v || !n || !origin || !url.data || !url.len)
        return 0;

    /* Ignore a literal fragment separator, not percent-encoded '#' bytes. */
    for (size_t i = 0; i < url.len; i++) {
        if (url.data[i] == '#') {
            url.len = i;
            break;
        }
    }

    size_t best = 0;
    uint64_t worker = 0;
    for (size_t i = 0; i < n; i++) {
        muba_span scope = v[i].scope;
        if (v[i].origin == origin && v[i].worker && scope.data &&
            scope.len > best && scope.len <= url.len &&
            memcmp(url.data, scope.data, scope.len) == 0) {
            best = scope.len;
            worker = v[i].worker;
        }
    }
    return worker;
}
#endif /* MULIB_WEB_SERVICEWORKER_H */
