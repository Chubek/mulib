/* MuLIB browser staging addition: publicsuffix
 * Implemented subset: Longest exact/wildcard/exception suffix match against caller-supplied rule list.
 * Remaining: PSL data ingestion, IDNA, private section policy and canonical validation.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_PUBLICSUFFIX_H
#define MULIB_PUBLICSUFFIX_H
#include "detail/mulib-browser-additions-common.h"

static inline size_t mulib_publicsuffix_labels(muba_span s){if(!s.len)return 0;size_t n=1;for(size_t i=0;i<s.len;i++)if(s.data[i]=='.')n++;return n;}
static inline int mulib_publicsuffix_suffix(muba_span host,muba_span rule){return rule.len&&host.len>=rule.len&&muba_ieq((muba_span){host.data+host.len-rule.len,rule.len},rule)&&(host.len==rule.len||host.data[host.len-rule.len-1]=='.');}
static inline size_t mulib_publicsuffix_count(muba_span host,const char *const *rules,size_t n){if(!muba_span_ok(host)||!host.len||(!rules&&n))return 0;size_t best=1;for(size_t i=0;i<n;i++){muba_span r=muba_str(rules[i]);int ex=r.len&&r.data[0]=='!';int wild=r.len>2&&r.data[0]=='*'&&r.data[1]=='.';if(ex){r.data++;r.len--;}if(wild){r.data+=2;r.len-=2;}if(mulib_publicsuffix_suffix(host,r)&&(!wild||host.len>r.len)){size_t x=mulib_publicsuffix_labels(r)+(unsigned)wild;if(ex)return x>1?x-1:1;if(x>best)best=x;}}return best;}
#endif /* MULIB_PUBLICSUFFIX_H */
