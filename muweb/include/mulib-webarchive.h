/** @file mulib-webarchive.h
 * @brief Write WARC-like minimal record headers into a bounded caller buffer.
 * C11 single-header implementation. Define MULIB_IMPL_WEBARCHIVE in one translation unit.
 */
#ifndef MULIB_WEBARCHIVE_H
#define MULIB_WEBARCHIVE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_webarchive_record_header(const char *url,size_t body_bytes,char *out,size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WEBARCHIVE) && !defined(MULIB_IMPL_WEBARCHIVE_IMPLEMENTED)
#define MULIB_IMPL_WEBARCHIVE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_webarchive_record_header(const char *url,size_t n,char *o,size_t cap){if(!url||!o||strchr(url,'\r')||strchr(url,'\n'))return 0;int z=snprintf(o,cap,"WARC/1.0\r\nWARC-Type: resource\r\nWARC-Target-URI: %s\r\nContent-Length: %zu\r\n\r\n",url,n);return z>=0&&(size_t)z<cap?(size_t)z:0;}

#ifdef __cplusplus
}
#endif
#endif
