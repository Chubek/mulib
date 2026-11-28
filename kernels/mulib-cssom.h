/* MuLIB browser staging addition: cssom
 * Implemented subset: Bounded ordered stylesheet rule handle insertion/removal.
 * Remaining: CSSRule hierarchy, parsing, origin-clean checks, adoption and live objects.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSSOM_H
#define MULIB_CSSOM_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uintptr_t *rules;size_t len,cap;int readonly;} mulib_cssom_sheet;
static inline int mulib_cssom_insert(mulib_cssom_sheet *s,size_t at,uintptr_t rule){if(!s||s->readonly||!rule||at>s->len||s->len>=s->cap)return 0;if(at<s->len)memmove(s->rules+at+1,s->rules+at,(s->len-at)*sizeof *s->rules);s->rules[at]=rule;s->len++;return 1;}
static inline int mulib_cssom_delete(mulib_cssom_sheet *s,size_t at){if(!s||s->readonly||at>=s->len)return 0;if(at+1<s->len)memmove(s->rules+at,s->rules+at+1,(s->len-at-1)*sizeof *s->rules);s->len--;return 1;}
#endif /* MULIB_CSSOM_H */
