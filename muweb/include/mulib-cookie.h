/** @file mulib-cookie.h
 * @brief HTTP Set-Cookie name/value validation and extraction.
 * C11 single-header implementation. Define MULIB_IMPL_COOKIE in one translation unit.
 */
#ifndef MULIB_COOKIE_H
#define MULIB_COOKIE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Copy initial cookie-pair from Set-Cookie, preserving raw value; rejects controls. */
int mulib_cookie_pair(const char *header,char *name,size_t nc,char *value,size_t vc);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_COOKIE) && !defined(MULIB_IMPL_COOKIE_IMPLEMENTED)
#define MULIB_IMPL_COOKIE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

int mulib_cookie_pair(const char *s,char *n,size_t nc,char *v,size_t vc){if(!s||!n||!v)return 0;const char *e=strchr(s,';');if(!e)e=s+strlen(s);const char *q=memchr(s,'=',(size_t)(e-s));if(!q||q==s)return 0;size_t a=(size_t)(q-s),b=(size_t)(e-q-1);if(a>=nc||b>=vc)return 0;for(const char *p=s;p<e;p++)if((unsigned char)*p<32||(unsigned char)*p==127)return 0;memcpy(n,s,a);n[a]=0;memcpy(v,q+1,b);v[b]=0;return 1;}

#ifdef __cplusplus
}
#endif
#endif
