/* MuLIB browser staging addition: web-workerclient
 * Implemented subset: Bounded client lookup and origin filtering.
 * Remaining: Client APIs, navigation, focus and message delivery.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_WORKERCLIENT_H
#define MULIB_WEB_WORKERCLIENT_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t id,origin;unsigned kind;int active;} mulib_web_workerclient_entry;
static inline const mulib_web_workerclient_entry *mulib_web_workerclient_find(const mulib_web_workerclient_entry *v,size_t n,uint64_t id,uint64_t origin){if(v)for(size_t i=0;i<n;i++)if(v[i].id==id&&v[i].origin==origin&&v[i].active)return v+i;return NULL;}
#endif /* MULIB_WEB_WORKERCLIENT_H */
