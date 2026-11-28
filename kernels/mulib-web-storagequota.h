/* MuLIB browser staging addition: web-storagequota
 * Implemented subset: Checked reservation and release accounting.
 * Remaining: Persistent origin quotas, eviction and storage manager.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_STORAGEQUOTA_H
#define MULIB_WEB_STORAGEQUOTA_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {size_t used,limit;} mulib_web_storagequota_t;
static inline int mulib_web_storagequota_reserve(mulib_web_storagequota_t *q,size_t n){if(!q||q->used>q->limit||n>q->limit-q->used)return 0;q->used+=n;return 1;}
static inline int mulib_web_storagequota_release(mulib_web_storagequota_t *q,size_t n){if(!q||n>q->used)return 0;q->used-=n;return 1;}
#endif /* MULIB_WEB_STORAGEQUOTA_H */
