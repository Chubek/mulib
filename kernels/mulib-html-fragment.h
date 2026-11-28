/* MuLIB browser staging addition: html-fragment
 * Implemented subset: Fragment tokenization through incremental parser with byte limit.
 * Remaining: Context-dependent insertion modes and native fragment construction.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_HTML_FRAGMENT_H
#define MULIB_HTML_FRAGMENT_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-html-parser.h"
static inline int mulib_html_fragment_scan(muba_span s,size_t budget,mulib_html_parser_emit_fn fn,void *ctx){mulib_html_parser_t p={0};p.budget=budget;size_t n;int r=mulib_html_parser_feed(&p,s,1,fn,ctx,&n);mulib_html_parser_free(&p);return r==1&&n==s.len;}
#endif /* MULIB_HTML_FRAGMENT_H */
