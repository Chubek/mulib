/* MuLIB browser staging addition: html-editing
 * Implemented subset: Transactional UTF-16 text splice on caller buffer.
 * Remaining: Selection, editing host, composition, undo and DOM input events.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_HTML_EDITING_H
#define MULIB_HTML_EDITING_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_html_editing_replace(uint16_t *v,size_t *n,size_t cap,size_t start,size_t end,const uint16_t *src,size_t sn){if(!n||(!v&&cap)||(!src&&sn)||*n>cap||start>end||end>*n||sn>cap-(*n-(end-start))||sn>SIZE_MAX/2||cap>SIZE_MAX/2)return 0;uint16_t *tmp=sn?malloc(sn*2):NULL;if(sn&&!tmp)return 0;if(sn)memcpy(tmp,src,sn*2);if(*n>end)memmove(v+start+sn,v+end,(*n-end)*2);if(sn)memcpy(v+start,tmp,sn*2);free(tmp);*n=*n-(end-start)+sn;return 1;}
#endif /* MULIB_HTML_EDITING_H */
