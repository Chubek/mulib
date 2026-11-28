/* MuLIB browser staging addition: web-structuredclone
 * Implemented subset: Owned byte copy and transfer of ArrayBuffer backing store.
 * Remaining: Graph traversal, cycles, transfer list validation, JS object types and realm creation.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_STRUCTUREDCLONE_H
#define MULIB_WEB_STRUCTUREDCLONE_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-microjs-buffer.h"
static inline int mulib_web_structuredclone_bytes(muba_span s,muba_buf *out){if(!out)return 0;muba_buf b={0};if(!muba_append(&b,s))return 0;return muba_commit(out,&b);}
static inline int mulib_web_structuredclone_transfer(mulib_microjs_buffer_t *src,mulib_microjs_buffer_t *dst){if(!src||!dst||src==dst||src->detached||dst->detached||dst->bytes.data)return 0;dst->bytes=src->bytes;memset(&src->bytes,0,sizeof src->bytes);src->detached=1;return 1;}
#endif /* MULIB_WEB_STRUCTUREDCLONE_H */
