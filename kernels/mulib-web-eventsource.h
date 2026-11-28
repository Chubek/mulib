/* MuLIB browser staging addition: web-eventsource
 * Implemented subset: SSE line field split and CR trimming.
 * Remaining: Event accumulation, reconnect, Last-Event-ID and Fetch integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_EVENTSOURCE_H
#define MULIB_WEB_EVENTSOURCE_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_web_eventsource_field(muba_span line,muba_span *name,muba_span *value){if(!name||!value||!muba_span_ok(line)||!line.len||line.data[0]==':')return 0;if(line.data[line.len-1]=='\r')line.len--;size_t i=0;while(i<line.len&&line.data[i]!=':')i++;*name=(muba_span){line.data,i};size_t j=i<line.len?i+1:i;if(j<line.len&&line.data[j]==' ')j++;*value=(muba_span){line.data+j,line.len-j};return 1;}
#endif /* MULIB_WEB_EVENTSOURCE_H */
