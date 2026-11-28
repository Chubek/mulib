/* MuLIB browser staging addition: web-storageevent
 * Implemented subset: Suppress same-document or different-storage-key event targets.
 * Remaining: Cross-document storage event scheduling and value snapshots.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_STORAGEEVENT_H
#define MULIB_WEB_STORAGEEVENT_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-web-storagekey.h"
static inline int mulib_web_storageevent_deliver(uint64_t source,uint64_t target,mulib_web_storagekey_t a,mulib_web_storagekey_t b){return source&&target&&source!=target&&mulib_web_storagekey_equal(a,b);}
#endif /* MULIB_WEB_STORAGEEVENT_H */
