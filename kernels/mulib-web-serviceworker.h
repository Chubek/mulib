/* MuLIB browser staging addition: web-serviceworker
 * Implemented subset: Registration scope longest-prefix lookup on serialized same-origin URLs.
 * Remaining: Install/activate/update lifecycle, Fetch interception, scope authorization.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_SERVICEWORKER_H
#define MULIB_WEB_SERVICEWORKER_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_span scope;uint64_t origin,worker;} mulib_web_serviceworker_registration;
static inline uint64_t mulib_web_serviceworker_match(const mulib_web_serviceworker_registration *v,size_t n,uint64_t origin,muba_span url){if(!v||!muba_span_ok(url))return 0;size_t best=0;uint64_t worker=0;for(size_t i=0;i<n;i++)if(v[i].origin==origin&&v[i].worker&&muba_span_ok(v[i].scope)&&v[i].scope.len>best&&v[i].scope.len<=url.len&&!memcmp(url.data,v[i].scope.data,v[i].scope.len)){best=v[i].scope.len;worker=v[i].worker;}return worker;}
#endif /* MULIB_WEB_SERVICEWORKER_H */
