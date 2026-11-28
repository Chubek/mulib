/* MuLIB browser staging addition: microjs-iterator
 * Implemented subset: Bounded next/close state and callback iterator.
 * Remaining: JS IteratorResult objects, async iterator and abrupt completion rules.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_MICROJS_ITERATOR_H
#define MULIB_MICROJS_ITERATOR_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_microjs_iterator_next_fn)(void *,uintptr_t *);typedef void (*mulib_microjs_iterator_close_fn)(void *);
typedef struct {mulib_microjs_iterator_next_fn next;mulib_microjs_iterator_close_fn close;void *ctx;int done;} mulib_microjs_iterator_t;
static inline int mulib_microjs_iterator_step(mulib_microjs_iterator_t *i,uintptr_t *v){if(!i||!v||!i->next)return -1;if(i->done)return 0;int r=i->next(i->ctx,v);if(r==0)i->done=1;return r;}
static inline void mulib_microjs_iterator_close(mulib_microjs_iterator_t *i){if(i&&!i->done){i->done=1;if(i->close)i->close(i->ctx);}}
#endif /* MULIB_MICROJS_ITERATOR_H */
