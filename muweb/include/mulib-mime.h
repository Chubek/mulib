/** @file mulib-mime.h
 * @brief Bounded Content-Type type/subtype and parameter stripping.
 * C11 single-header implementation. Define MULIB_IMPL_MIME in one translation unit.
 */
#ifndef MULIB_MIME_H
#define MULIB_MIME_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Copy lowercase type/subtype without parameters. Return 1 or 0 for invalid. */
int mulib_mime_type(const char *header,char *out,size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MIME) && !defined(MULIB_IMPL_MIME_IMPLEMENTED)
#define MULIB_IMPL_MIME_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

int mulib_mime_type(const char *s,char *o,size_t cap){if(!s||!o||cap<4)return 0;while(*s==' '||*s=='\t')s++;size_t n=0;int slash=0;while(*s&&*s!=';'&&*s!=' '&&*s!='\t'){unsigned char c=(unsigned char)*s++;if(c=='/')slash++;else if(!isalnum(c)&&c!='-'&&c!='.'&&c!='+')return 0;if(n+1>=cap)return 0;o[n++]=(char)tolower(c);}if(!slash||slash!=1||!n||o[n-1]=='/')return 0;o[n]=0;return 1;}

#ifdef __cplusplus
}
#endif
#endif
