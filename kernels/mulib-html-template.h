/* MuLIB browser staging addition: html-template
 * Implemented subset: Template fragment handle lifecycle; explicit ownership transfer.
 * Remaining: Inert owner documents, cloning and declarative shadow roots.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_HTML_TEMPLATE_H
#define MULIB_HTML_TEMPLATE_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uintptr_t fragment;int inert;} mulib_html_template_t;
static inline int mulib_html_template_init(mulib_html_template_t *t,uintptr_t f){if(!t||!f||t->fragment)return 0;*t=(mulib_html_template_t){f,1};return 1;}
static inline uintptr_t mulib_html_template_take(mulib_html_template_t *t){if(!t)return 0;uintptr_t f=t->fragment;t->fragment=0;return f;}
#endif /* MULIB_HTML_TEMPLATE_H */
