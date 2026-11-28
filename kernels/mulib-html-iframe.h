/* MuLIB browser staging addition: html-iframe
 * Implemented subset: Frame metadata and child attachment validation.
 * Remaining: Navigation, sandbox, WindowProxy and nested contexts integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_HTML_IFRAME_H
#define MULIB_HTML_IFRAME_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t parent,child;unsigned sandbox;int attached;} mulib_html_iframe_t;
static inline int mulib_html_iframe_attach(mulib_html_iframe_t *f,uint64_t parent,uint64_t child,unsigned flags){if(!f||f->attached||!parent||!child||parent==child)return 0;*f=(mulib_html_iframe_t){parent,child,flags,1};return 1;}
static inline void mulib_html_iframe_detach(mulib_html_iframe_t *f){if(f)memset(f,0,sizeof *f);}
#endif /* MULIB_HTML_IFRAME_H */
