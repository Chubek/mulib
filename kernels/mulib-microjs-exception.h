/* MuLIB browser staging addition: microjs-exception
 * Implemented subset: Completion records and finally override rule.
 * Remaining: Exception objects, stack unwinding and abrupt completion integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_MICROJS_EXCEPTION_H
#define MULIB_MICROJS_EXCEPTION_H
#include "detail/mulib-browser-additions-common.h"

typedef enum {mulib_microjs_exception_NORMAL,mulib_microjs_exception_RETURN,mulib_microjs_exception_THROW,mulib_microjs_exception_BREAK,mulib_microjs_exception_CONTINUE} mulib_microjs_exception_kind;
typedef struct {mulib_microjs_exception_kind kind;uintptr_t value;uint64_t target;} mulib_microjs_exception_completion;
static inline mulib_microjs_exception_completion mulib_microjs_exception_finally(mulib_microjs_exception_completion prior,mulib_microjs_exception_completion cleanup){return cleanup.kind==mulib_microjs_exception_NORMAL?prior:cleanup;}
static inline int mulib_microjs_exception_abrupt(mulib_microjs_exception_completion c){return c.kind!=mulib_microjs_exception_NORMAL;}
#endif /* MULIB_MICROJS_EXCEPTION_H */
