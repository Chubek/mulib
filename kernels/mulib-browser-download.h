/* MuLIB browser staging addition: browser-download
 * Implemented subset: Bounded byte progress and terminal finish/cancel state.
 * Remaining: Filename sanitization, network/storage providers and permissions.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_DOWNLOAD_H
#define MULIB_BROWSER_DOWNLOAD_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t received,limit;unsigned state;} mulib_browser_download_t;
static inline int mulib_browser_download_charge(mulib_browser_download_t *d,uint64_t n){if(!d||d->state||d->received>d->limit||n>d->limit-d->received)return 0;d->received+=n;return 1;}
static inline int mulib_browser_download_finish(mulib_browser_download_t *d,int success){if(!d||d->state)return 0;d->state=success?1u:2u;return 1;}
#endif /* MULIB_BROWSER_DOWNLOAD_H */
