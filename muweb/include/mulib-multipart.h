/** @file mulib-multipart.h
 * @brief Multipart/form-data boundary and field formatting for simple text fields.
 * C11 single-header implementation. Define MULIB_IMPL_MULTIPART in one translation unit.
 */
#ifndef MULIB_MULTIPART_H
#define MULIB_MULTIPART_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Format single text field into caller buffer; returns byte length or 0 on error. */
size_t mulib_multipart_field(const char *boundary,const char *name,const char *value,char *out,size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MULTIPART) && !defined(MULIB_IMPL_MULTIPART_IMPLEMENTED)
#define MULIB_IMPL_MULTIPART_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_multipart_field(const char *b,const char *n,const char *v,char *o,size_t cap){if(!b||!n||!v||!o)return 0;for(const char *p=b;*p;p++)if(!isalnum((unsigned char)*p)&&*p!='-'&&*p!='_')return 0;for(const char *p=n;*p;p++)if(*p=='\r'||*p=='\n'||*p=='\"')return 0;int z=snprintf(o,cap,"--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n",b,n,v);return z>=0&&(size_t)z<cap?(size_t)z:0;}

#ifdef __cplusplus
}
#endif
#endif
