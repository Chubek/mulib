/* MuLIB browser staging addition: browser-paintstream
 * Implemented subset: Backend-neutral paint command payload publication with owned bytes and fixed staging channel.
 * Remaining: Drawing command schema, clipping, resource handles and ordering.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_PAINTSTREAM_H
#define MULIB_BROWSER_PAINTSTREAM_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-browser-journal.h"
static inline int mulib_browser_paintstream_emit(mulib_browser_journal_t *j,unsigned kind,uint64_t document,uint64_t node,muba_span payload){return mulib_browser_journal_append(j,8,kind,document,node,payload);}
#endif /* MULIB_BROWSER_PAINTSTREAM_H */
