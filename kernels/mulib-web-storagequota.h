/* MuLIB browser staging addition: web-storagequota
 * Implemented subset: Checked reservation, release and replacement accounting.
 * Remaining: Persistent origin quotas, eviction and storage manager.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_STORAGEQUOTA_H
#define MULIB_WEB_STORAGEQUOTA_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {size_t used,limit;} mulib_web_storagequota_t;
/* Replace a previous charge; failure preserves used. A reduced limit
 * may be below used: permit nonincreasing changes to let usage drain. */
static inline int mulib_web_storagequota_replace(mulib_web_storagequota_t *q,
    size_t old_charge, size_t new_charge)
{
    if (!q || old_charge > q->used)
        return 0;
    size_t remaining = q->used - old_charge;
    size_t ceiling = new_charge > old_charge ? q->limit : q->used;
    if (remaining > ceiling || new_charge > ceiling - remaining)
        return 0;
    q->used = remaining + new_charge;
    return 1;
}

static inline int mulib_web_storagequota_reserve(mulib_web_storagequota_t *q,size_t n)
{
    return q && q->used <= q->limit && mulib_web_storagequota_replace(q, 0, n);
}

static inline int mulib_web_storagequota_release(mulib_web_storagequota_t *q,size_t n)
{
    return mulib_web_storagequota_replace(q, n, 0);
}
#endif /* MULIB_WEB_STORAGEQUOTA_H */
