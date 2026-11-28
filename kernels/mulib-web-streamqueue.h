/* MuLIB browser staging addition: web-streamqueue
 * Implemented subset: High-water-mark desired size and checked queue accounting.
 * Remaining: Chunk size algorithms, promises, pull scheduling and cancellation.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_STREAMQUEUE_H
#define MULIB_WEB_STREAMQUEUE_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {size_t size,high;} mulib_web_streamqueue_t;
static inline int mulib_web_streamqueue_add(mulib_web_streamqueue_t *q,size_t n){if(!q||n>SIZE_MAX-q->size)return 0;q->size+=n;return 1;}
static inline int mulib_web_streamqueue_remove(mulib_web_streamqueue_t *q,size_t n){if(!q||n>q->size)return 0;q->size-=n;return 1;}
static inline double mulib_web_streamqueue_desired(const mulib_web_streamqueue_t *q){return q?(double)q->high-(double)q->size:0;}
#endif /* MULIB_WEB_STREAMQUEUE_H */
