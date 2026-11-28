/* MuLIB browser staging addition: html-entities
 * Implemented subset: HTML5 named reference table (including legacy spellings), longest matching, attribute ambiguity checks and numeric scalar/remapping rules.
 * Remaining: Tokenizer integration, streaming, parse error reporting and differential conformance.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_HTML_ENTITIES_H
#define MULIB_HTML_ENTITIES_H
#include "detail/mulib-browser-additions-common.h"
#include "detail/mulib-html-named-data.inc"
static inline const char *mulib_html_entities_lookup(muba_span name,uint32_t *a,uint32_t *b)
{
    size_t lo=0, hi=sizeof muba_html_named/sizeof *muba_html_named;
    while(lo<hi){size_t mid=lo+(hi-lo)/2;const char *key=muba_html_named[mid].name;
        size_t kn=strlen(key), n=kn<name.len?kn:name.len;
        int c=n?memcmp(name.data,key,n):0;
        if(!c)c=(name.len>kn)-(name.len<kn);
        if(c<0)hi=mid;else if(c>0)lo=mid+1;
        else{*a=muba_html_named[mid].first;*b=muba_html_named[mid].second;return key;}}
    return NULL;
}
static inline uint32_t mulib_html_entities_numeric(uint32_t c)
{
    static const uint16_t replacements[32]={0x20ac,0x81,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,0x2c6,0x2030,0x160,0x2039,0x152,0x8d,0x17d,0x8f,0x90,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0x9d,0x17e,0x178};
    if(!c||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return 0xfffd;
    if(c>=128&&c<160)return replacements[c-128];
    return c;
}
static inline int mulib_html_entities_decode_context(muba_span s,int attribute,muba_buf *out)
{
    if(!out||!muba_span_ok(s))return 0;
    muba_buf b={0};size_t i=0;
    while(i<s.len){
        if(s.data[i]=='&'&&s.len-i>1){
            size_t start=i+1,j=start;
            if(s.data[j]=='#'){
                j++;unsigned base=10;if(j<s.len&&muba_lower(s.data[j])=='x'){j++;base=16;}
                size_t first=j;uint32_t c=0;
                while(j<s.len){int d=muba_hex(s.data[j]);if(d<0||(unsigned)d>=base)break;
                    if(c>(0x110000u-(unsigned)d)/base)c=0x110000;else c=c*base+(unsigned)d;j++;}
                if(j>first){if(j<s.len&&s.data[j]==';')j++;
                    if(!muba_utf8_put(&b,mulib_html_entities_numeric(c),0))goto fail;
                    i=j;continue;}
            }else{
                size_t max=s.len-start;if(max>32)max=32;
                for(size_t n=max;n;n--){uint32_t a=0,c=0;
                    if(!mulib_html_entities_lookup((muba_span){s.data+start,n},&a,&c))continue;
                    j=start+n;
                    if(attribute&&s.data[j-1]!=';'&&j<s.len&&(muba_alpha(s.data[j])||muba_digit(s.data[j])||s.data[j]=='='))break;
                    if(!muba_utf8_put(&b,a,0)||(c&&!muba_utf8_put(&b,c,0)))goto fail;
                    i=j;break;}
                if(i>start)continue;
            }
        }
        if(!muba_byte(&b,s.data[i++]))goto fail;
    }
    return muba_commit(out,&b);
fail:muba_free(&b);return 0;
}
static inline int mulib_html_entities_decode(muba_span s,muba_buf *out){return mulib_html_entities_decode_context(s,0,out);}
#endif /* MULIB_HTML_ENTITIES_H */
