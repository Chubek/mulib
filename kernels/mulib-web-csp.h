/* MuLIB browser staging addition: web-csp
 * Implemented subset: Tokenized directive lookup, first duplicate wins. No authorization decision.
 * Remaining: Full source expressions, nonce/hash semantics, enforcement and reporting.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_CSP_H
#define MULIB_WEB_CSP_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_web_csp_directive(muba_span policy,muba_span name,muba_span *out){if(!out||!muba_span_ok(policy)||!muba_span_ok(name))return 0;for(size_t i=0;i<policy.len;){size_t j=i;while(j<policy.len&&policy.data[j]!=';')j++;muba_span d=muba_trim((muba_span){policy.data+i,j-i});size_t k=0;while(k<d.len&&!muba_space(d.data[k]))k++;if(muba_ieq((muba_span){d.data,k},name)){*out=muba_trim((muba_span){d.data+k,d.len-k});return 1;}i=j<policy.len?j+1:j;}return 0;}
#endif /* MULIB_WEB_CSP_H */
