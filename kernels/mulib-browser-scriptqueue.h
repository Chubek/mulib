/* MuLIB browser staging addition: browser-scriptqueue
 * Implemented subset: Choose ready async or parser-blocking work, or ordered defer after parse.
 * Remaining: Stylesheet blocking, module dependencies, task source and actual execution.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_SCRIPTQUEUE_H
#define MULIB_BROWSER_SCRIPTQUEUE_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-html-script.h"
typedef struct {uint64_t id;mulib_html_script_mode mode;int ready,done;} mulib_browser_scriptqueue_entry;
static inline size_t mulib_browser_scriptqueue_next(const mulib_browser_scriptqueue_entry *v,size_t n,int parsed){if(!v)return SIZE_MAX;for(size_t i=0;i<n;i++)if(!v[i].done&&v[i].ready&&(v[i].mode==mulib_html_script_ASYNC||v[i].mode==mulib_html_script_IMMEDIATE))return i;for(size_t i=0;i<n;i++)if(!v[i].done&&v[i].mode==mulib_html_script_PARSER_BLOCK)return v[i].ready?i:SIZE_MAX;if(parsed)for(size_t i=0;i<n;i++)if(!v[i].done&&v[i].mode==mulib_html_script_DEFER)return v[i].ready?i:SIZE_MAX;return SIZE_MAX;}
#endif /* MULIB_BROWSER_SCRIPTQUEUE_H */
