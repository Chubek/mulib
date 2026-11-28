/* MuLIB browser staging addition: fetch-body
 * Implemented subset: Owned byte body with one-shot consume and explicit release.
 * Remaining: Streaming, tee, body mixin and promise integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_FETCH_BODY_H
#define MULIB_FETCH_BODY_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_buf data;int used;} mulib_fetch_body_t;
static inline int mulib_fetch_body_init(mulib_fetch_body_t *b,muba_span s){if(!b||b->used||b->data.data)return 0;return muba_append(&b->data,s);}
static inline int mulib_fetch_body_consume(mulib_fetch_body_t *b,muba_buf *out){if(!b||!out||b->used||out==&b->data)return 0;b->used=1;return muba_commit(out,&b->data);}
static inline void mulib_fetch_body_free(mulib_fetch_body_t *b){if(b){muba_free(&b->data);b->used=1;}}
#endif /* MULIB_FETCH_BODY_H */
