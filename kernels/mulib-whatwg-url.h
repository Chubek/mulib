/* MuLIB browser staging addition: whatwg-url
 * Implemented subset: Strict ASCII HTTP(S) URL decomposition into borrowed scheme/authority/path/query/fragment.
 * Remaining: WHATWG state machine, relative resolution, IDNA, special schemes and URL serialization.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WHATWG_URL_H
#define MULIB_WHATWG_URL_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_span scheme,authority,path,query,fragment;} mulib_whatwg_url_t;
static inline int mulib_whatwg_url_parse(muba_span s,mulib_whatwg_url_t *out){if(!out||!muba_span_ok(s))return 0;for(size_t i=0;i<s.len;i++)if(s.data[i]<=32||s.data[i]>=127||s.data[i]=='\\')return 0;size_t i=0;while(i<s.len&&s.data[i]!=':')i++;if(i==s.len||s.len-i<3||s.data[i+1]!='/'||s.data[i+2]!='/')return 0;mulib_whatwg_url_t r={0};r.scheme=(muba_span){s.data,i};if(!muba_ieq(r.scheme,muba_str("http"))&&!muba_ieq(r.scheme,muba_str("https")))return 0;i+=3;size_t j=i;while(i<s.len&&s.data[i]!='/'&&s.data[i]!='?'&&s.data[i]!='#'){if(s.data[i]=='@')return 0;i++;}if(i==j)return 0;r.authority=(muba_span){s.data+j,i-j};j=i;while(i<s.len&&s.data[i]!='?'&&s.data[i]!='#')i++;r.path=(muba_span){s.data+j,i-j};if(i<s.len&&s.data[i]=='?'){j=++i;while(i<s.len&&s.data[i]!='#')i++;r.query=(muba_span){s.data+j,i-j};}if(i<s.len)r.fragment=(muba_span){s.data+i+1,s.len-i-1};*out=r;return 1;}
#endif /* MULIB_WHATWG_URL_H */
