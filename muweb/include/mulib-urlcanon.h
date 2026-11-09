/** @file mulib-urlcanon.h
 * @brief Conservative URL normalization for HTTP(S) resource keys.
 * C11 single-header implementation. Define MULIB_IMPL_URLCANON in one translation unit.
 */
#ifndef MULIB_URLCANON_H
#define MULIB_URLCANON_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Normalize ASCII scheme and host; strip fragment and default :80/:443 port. Does not resolve relative references, IDNA or dot segments. Returns 1 on success. */
int mulib_urlcanon_normalize(const char *src, char *dst, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_URLCANON) && !defined(MULIB_IMPL_URLCANON_IMPLEMENTED)
#define MULIB_IMPL_URLCANON_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

int mulib_urlcanon_normalize(const char *s,char *o,size_t cap){
 if(!s||!o||!cap)return 0;
 const char *p=strstr(s,"://"); if(!p || ((size_t)(p-s)!=4 && (size_t)(p-s)!=5))return 0;
 size_t scheme=(size_t)(p-s); int https=scheme==5;
 if(https ? !(tolower((unsigned char)s[0])=='h'&&tolower((unsigned char)s[1])=='t'&&tolower((unsigned char)s[2])=='t'&&tolower((unsigned char)s[3])=='p'&&tolower((unsigned char)s[4])=='s') : !(tolower((unsigned char)s[0])=='h'&&tolower((unsigned char)s[1])=='t'&&tolower((unsigned char)s[2])=='t'&&tolower((unsigned char)s[3])=='p'))return 0;
 const char *host=p+3,*path=host;while(*path&&*path!='/'&&*path!='?'&&*path!='#')path++;
 if(path==host || memchr(host,'@',(size_t)(path-host)))return 0;
 const char *end=strchr(path,'#');if(!end)end=s+strlen(s);
 const char *port=path; while(port>host && port[-1]!=':')port--;
 size_t hlen=(size_t)(path-host),tail=(size_t)(end-path);
 if(port>host){const char *ps=port;int def=https?(path-ps==3&&!memcmp(ps,"443",3)):(path-ps==2&&!memcmp(ps,"80",2));if(def)hlen=(size_t)(ps-host)-1;}
 size_t n=scheme+3+hlen+tail+(path==end||*path=='?'?1:0);
 if(n+1>cap)return 0;
 size_t k=0;for(size_t i=0;i<scheme;i++)o[k++]=(char)tolower((unsigned char)s[i]);memcpy(o+k,"://",3);k+=3;
 for(size_t i=0;i<hlen;i++)o[k++]=(char)tolower((unsigned char)host[i]);
 if(path==end||*path=='?'){o[k++]='/';}memcpy(o+k,path,tail);k+=tail;o[k]=0;return 1;
}

#ifdef __cplusplus
}
#endif
#endif
