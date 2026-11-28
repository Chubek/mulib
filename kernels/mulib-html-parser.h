/* MuLIB browser staging addition: html-parser
 * Implemented subset: Incremental ASCII tag/text tokenizer with quoted attribute awareness, callback pause and byte budget.
 * Remaining: WHATWG tokenizer states, raw text, comments, tree building, native DOM and parser-inserted scripts.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_HTML_PARSER_H
#define MULIB_HTML_PARSER_H
#include "detail/mulib-browser-additions-common.h"

typedef enum {mulib_html_parser_TEXT,mulib_html_parser_START,mulib_html_parser_END} mulib_html_parser_kind;
typedef int (*mulib_html_parser_emit_fn)(void *,mulib_html_parser_kind,muba_span);
typedef struct {muba_buf pending;size_t budget,seen;int in_tag;unsigned char quote;} mulib_html_parser_t;
static inline void mulib_html_parser_free(mulib_html_parser_t *p){if(p){muba_free(&p->pending);memset(p,0,sizeof *p);}}
/* Returns 1 complete, 0 paused (token delivered), -1 failure; consumed includes delivered token. */
static inline int mulib_html_parser_feed(mulib_html_parser_t *p,muba_span s,int final,mulib_html_parser_emit_fn emit,void *ctx,size_t *consumed){if(!p||!emit||!consumed||!muba_span_ok(s))return -1;*consumed=0;while(*consumed<s.len){unsigned char c=s.data[*consumed];if(!p->in_tag&&c=='<'&&p->pending.len){int r=emit(ctx,mulib_html_parser_TEXT,(muba_span){p->pending.data,p->pending.len});p->pending.len=0;if(!r)return 0;}if(p->seen>=p->budget||!muba_byte(&p->pending,c))return -1;p->seen++;(*consumed)++;if(!p->in_tag&&c=='<'){p->in_tag=1;continue;}if(p->in_tag){if(p->quote){if(c==p->quote)p->quote=0;}else if(c=='\''||c=='"')p->quote=c;else if(c=='>'){int r=emit(ctx,p->pending.len>1&&p->pending.data[1]=='/'?mulib_html_parser_END:mulib_html_parser_START,(muba_span){p->pending.data,p->pending.len});p->pending.len=0;p->in_tag=0;if(!r)return 0;}}}if(final&&p->pending.len){int r=emit(ctx,mulib_html_parser_TEXT,(muba_span){p->pending.data,p->pending.len});p->pending.len=0;p->in_tag=0;p->quote=0;if(!r)return 0;}return 1;}
#endif /* MULIB_HTML_PARSER_H */
