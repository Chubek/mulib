/** @file mulib-robots.h
 * @brief Simple robots.txt User-agent and Disallow matcher for a selected bot.
 * C11 single-header implementation. Define MULIB_IMPL_ROBOTS in one translation unit.
 */
#ifndef MULIB_ROBOTS_H
#define MULIB_ROBOTS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Basic prefix-based robots rules, case-insensitive agent name; no wildcard or longest-match Allow support. 1 allowed, 0 disallowed. */
int mulib_robots_allowed(const char *robots,const char *agent,const char *path);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ROBOTS) && !defined(MULIB_IMPL_ROBOTS_IMPLEMENTED)
#define MULIB_IMPL_ROBOTS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

static int mulib_robots_eq_(const char*a,size_t n,const char*b){size_t m=strlen(b);if(n!=m)return 0;for(size_t i=0;i<n;i++)if(tolower((unsigned char)a[i])!=tolower((unsigned char)b[i]))return 0;return 1;}
int mulib_robots_allowed(const char *r,const char *agent,const char *path){if(!r||!agent||!path)return 0;int active=0,deny=0;size_t best=0;while(*r){const char *e=strchr(r,'\n');if(!e)e=r+strlen(r);const char *a=r;while(a<e&&isspace((unsigned char)*a))a++;const char *c=memchr(a,':',(size_t)(e-a));if(c){const char *v=c+1;while(v<e&&(*v==' '||*v=='\t'))v++;const char *z=e;while(z>v&&isspace((unsigned char)z[-1]))z--;if(mulib_robots_eq_(a,(size_t)(c-a),"User-agent"))active=mulib_robots_eq_(v,(size_t)(z-v),agent)||mulib_robots_eq_(v,(size_t)(z-v),"*");else if(active&&mulib_robots_eq_(a,(size_t)(c-a),"Disallow")&&z>v){size_t m=(size_t)(z-v);if(m>best&&!strncmp(path,v,m)){best=m;deny=1;}}else if(active&&mulib_robots_eq_(a,(size_t)(c-a),"Allow")&&z>v){size_t m=(size_t)(z-v);if(m>=best&&!strncmp(path,v,m)){best=m;deny=0;}}}r=*e?e+1:e;}return !deny;}

#ifdef __cplusplus
}
#endif
#endif
