/** @file mulib-typecore.h
 * @brief Small, allocation-checked primitives for standalone MuLIB publishing modules.
 * Define MULIB_IMPL_TYPECORE in exactly one translation unit.
 * C11 with libc only; no third-party link dependencies.
 */
#ifndef MULIB_TYPECORE_H
#define MULIB_TYPECORE_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_typecore_buf { unsigned char *data; size_t size, capacity; } mulib_typecore_buf_t;
typedef struct mulib_typecore_slice { const unsigned char *data; size_t size; } mulib_typecore_slice_t;
void mulib_typecore_free(mulib_typecore_buf_t *b);
int mulib_typecore_reserve(mulib_typecore_buf_t *b, size_t additional);
int mulib_typecore_append(mulib_typecore_buf_t *b, const void *bytes, size_t length);
int mulib_typecore_text(mulib_typecore_buf_t *b, const char *text);
int mulib_typecore_byte(mulib_typecore_buf_t *b, unsigned byte);
int mulib_typecore_u16be(mulib_typecore_buf_t *b, uint16_t v);
int mulib_typecore_u16le(mulib_typecore_buf_t *b, uint16_t v);
int mulib_typecore_u32be(mulib_typecore_buf_t *b, uint32_t v);
int mulib_typecore_u32le(mulib_typecore_buf_t *b, uint32_t v);
int mulib_typecore_read16be(mulib_typecore_slice_t s, size_t at, uint16_t *out);
int mulib_typecore_read32be(mulib_typecore_slice_t s, size_t at, uint32_t *out);
int mulib_typecore_read32le(mulib_typecore_slice_t s, size_t at, uint32_t *out);
uint32_t mulib_typecore_crc32(const void *bytes, size_t length);
uint32_t mulib_typecore_adler32(const void *bytes, size_t length);
int mulib_typecore_utf8(const unsigned char *s, size_t n, size_t *offset, uint32_t *cp);
int mulib_typecore_html(mulib_typecore_buf_t *out, const char *src, size_t n);
int mulib_typecore_latex(mulib_typecore_buf_t *out, const char *src, size_t n);
int mulib_typecore_json(mulib_typecore_buf_t *out, const char *src, size_t n);
int mulib_typecore_ends(mulib_typecore_buf_t *out); /* append NUL without changing size */
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TYPECORE) && !defined(MULIB_TYPECORE_IMPLEMENTED)
#define MULIB_TYPECORE_IMPLEMENTED
#ifndef MULIB_TYPECORE_REALLOC
#define MULIB_TYPECORE_REALLOC realloc
#endif
#ifndef MULIB_TYPECORE_FREE
#define MULIB_TYPECORE_FREE free
#endif
#ifdef __cplusplus
extern "C" {
#endif
void mulib_typecore_free(mulib_typecore_buf_t *b){if(b){MULIB_TYPECORE_FREE(b->data);memset(b,0,sizeof(*b));}}
int mulib_typecore_reserve(mulib_typecore_buf_t *b,size_t extra){
 if(!b||b->size>b->capacity||(!b->data&&b->capacity)||extra>SIZE_MAX-b->size)return 0;
 size_t need=b->size+extra;if(need<=b->capacity)return 1;
 size_t cap=b->capacity?b->capacity:64;
 while(cap<need){if(cap>SIZE_MAX/2){cap=need;break;}cap*=2;}
 void *p=MULIB_TYPECORE_REALLOC(b->data,cap);if(!p)return 0;b->data=(unsigned char*)p;b->capacity=cap;return 1;
}
int mulib_typecore_append(mulib_typecore_buf_t *b,const void *s,size_t n){
 if(!b||(!s&&n))return 0;
 /* If aliased source may be invalidated by realloc, use its offset. */
 size_t offset=0;int aliased=0;
 if(n&&b->data){uintptr_t p=(uintptr_t)s,lo=(uintptr_t)b->data;
   if(p>=lo&&p-lo<b->capacity){offset=(size_t)(p-lo);if(n>b->capacity-offset)return 0;aliased=1;}}
 if(!mulib_typecore_reserve(b,n))return 0;
 if(n){const void *src=aliased?(const void*)(b->data+offset):s;memmove(b->data+b->size,src,n);}
 b->size+=n;return 1;
}
int mulib_typecore_text(mulib_typecore_buf_t *b,const char *s){return s?mulib_typecore_append(b,s,strlen(s)):0;}
int mulib_typecore_byte(mulib_typecore_buf_t *b,unsigned v){if(v>255u)return 0;unsigned char c=(unsigned char)v;return mulib_typecore_append(b,&c,1);}
int mulib_typecore_u16be(mulib_typecore_buf_t *b,uint16_t v){unsigned char d[2]={(unsigned char)(v>>8),(unsigned char)v};return mulib_typecore_append(b,d,2);}
int mulib_typecore_u16le(mulib_typecore_buf_t *b,uint16_t v){unsigned char d[2]={(unsigned char)v,(unsigned char)(v>>8)};return mulib_typecore_append(b,d,2);}
int mulib_typecore_u32be(mulib_typecore_buf_t *b,uint32_t v){unsigned char d[4]={(unsigned char)(v>>24),(unsigned char)(v>>16),(unsigned char)(v>>8),(unsigned char)v};return mulib_typecore_append(b,d,4);}
int mulib_typecore_u32le(mulib_typecore_buf_t *b,uint32_t v){unsigned char d[4]={(unsigned char)v,(unsigned char)(v>>8),(unsigned char)(v>>16),(unsigned char)(v>>24)};return mulib_typecore_append(b,d,4);}
int mulib_typecore_read16be(mulib_typecore_slice_t s,size_t at,uint16_t *o){if(!o||at>s.size||s.size-at<2||!s.data)return 0;*o=(uint16_t)(((uint16_t)s.data[at]<<8)|s.data[at+1]);return 1;}
int mulib_typecore_read32be(mulib_typecore_slice_t s,size_t at,uint32_t *o){if(!o||at>s.size||s.size-at<4||!s.data)return 0;*o=((uint32_t)s.data[at]<<24)|((uint32_t)s.data[at+1]<<16)|((uint32_t)s.data[at+2]<<8)|s.data[at+3];return 1;}
int mulib_typecore_read32le(mulib_typecore_slice_t s,size_t at,uint32_t *o){if(!o||at>s.size||s.size-at<4||!s.data)return 0;*o=(uint32_t)s.data[at]|((uint32_t)s.data[at+1]<<8)|((uint32_t)s.data[at+2]<<16)|((uint32_t)s.data[at+3]<<24);return 1;}
uint32_t mulib_typecore_crc32(const void *p,size_t n){if(!p&&n)return 0;const unsigned char *s=(const unsigned char*)p;uint32_t c=0xffffffffu;for(size_t i=0;i<n;i++){c^=s[i];for(int j=0;j<8;j++)c=(c>>1)^(0xedb88320u& (uint32_t)-(int)(c&1u));}return ~c;}
uint32_t mulib_typecore_adler32(const void *p,size_t n){if(!p&&n)return 0;const unsigned char *s=(const unsigned char*)p;uint32_t a=1,b=0;for(size_t i=0;i<n;i++){a=(a+s[i])%65521u;b=(b+a)%65521u;}return b<<16|a;}
int mulib_typecore_utf8(const unsigned char *s,size_t n,size_t *at,uint32_t *out){
 if(!s||!at||!out||*at>=n)return 0;
 size_t i=*at;unsigned c=s[i];
 if(c<0x80){*out=c;*at=i+1;return 1;}
 unsigned need=(c>=0xc2&&c<=0xdf)?2:(c>=0xe0&&c<=0xef)?3:(c>=0xf0&&c<=0xf4)?4:0;
 if(!need||need>n-i)return 0;
 uint32_t cp=c&((1u<<(7-need))-1u);
 for(unsigned j=1;j<need;j++){unsigned b=s[i+j];if((b&0xc0)!=0x80)return 0;cp=(cp<<6)|(b&63u);}
 if(cp< (need==2?0x80u:need==3?0x800u:0x10000u)||cp>0x10ffffu||(cp>=0xd800u&&cp<=0xdfffu))return 0;
 *at=i+need;*out=cp;return 1;
}
static int mulib_typecore_html_(mulib_typecore_buf_t *out,const char *s,size_t n){if(!out||(!s&&n))return 0;for(size_t i=0;i<n;i++){const char *r=s[i]=='&'?"&amp;":s[i]=='<'?"&lt;":s[i]=='>'?"&gt;":s[i]=='\"'?"&quot;":NULL;if(r){if(!mulib_typecore_text(out,r))return 0;}else if(!mulib_typecore_byte(out,(unsigned char)s[i]))return 0;}return 1;}
static int mulib_typecore_latex_(mulib_typecore_buf_t *out,const char *s,size_t n){if(!out||(!s&&n))return 0;for(size_t i=0;i<n;i++){const char *r=NULL;switch(s[i]){case '\\':r="\\textbackslash{}";break;case '{':r="\\{";break;case '}':r="\\}";break;case '#':r="\\#";break;case '$':r="\\$";break;case '%':r="\\%";break;case '&':r="\\&";break;case '_':r="\\_";break;case '^':r="\\textasciicircum{}";break;case '~':r="\\textasciitilde{}";break;default:break;}if(r){if(!mulib_typecore_text(out,r))return 0;}else if(!mulib_typecore_byte(out,(unsigned char)s[i]))return 0;}return 1;}
static int mulib_typecore_json_(mulib_typecore_buf_t *out,const char *s,size_t n){if(!out||(!s&&n))return 0;const char *h="0123456789abcdef";for(size_t i=0;i<n;i++){unsigned char c=(unsigned char)s[i];if(c=='\"'||c=='\\'){if(!mulib_typecore_byte(out,'\\')||!mulib_typecore_byte(out,c))return 0;}else if(c<32){unsigned char esc[6]={'\\','u','0','0',(unsigned char)h[c>>4],(unsigned char)h[c&15]};if(!mulib_typecore_append(out,esc,6))return 0;}else if(!mulib_typecore_byte(out,c))return 0;}return 1;}
static int mulib_typecore_escape_(mulib_typecore_buf_t *out,const char *s,size_t n,
                                  int (*encode)(mulib_typecore_buf_t*,const char*,size_t)) {
    mulib_typecore_buf_t temporary={0};
    if(!out||(!s&&n)||!mulib_typecore_reserve(out,0))return 0;
    int ok=encode(&temporary,s,n)&&mulib_typecore_append(out,temporary.data,temporary.size);
    mulib_typecore_free(&temporary);return ok;
}
int mulib_typecore_html(mulib_typecore_buf_t *out,const char *s,size_t n){return mulib_typecore_escape_(out,s,n,mulib_typecore_html_);}
int mulib_typecore_latex(mulib_typecore_buf_t *out,const char *s,size_t n){return mulib_typecore_escape_(out,s,n,mulib_typecore_latex_);}
int mulib_typecore_json(mulib_typecore_buf_t *out,const char *s,size_t n){return mulib_typecore_escape_(out,s,n,mulib_typecore_json_);}
int mulib_typecore_ends(mulib_typecore_buf_t *out){if(!out||!mulib_typecore_reserve(out,1))return 0;out->data[out->size]=0;return 1;}
#ifdef __cplusplus
}
#endif
#endif
