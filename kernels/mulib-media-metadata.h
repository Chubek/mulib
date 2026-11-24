/** @file mulib-media-metadata.h
 * Borrowed key/value metadata with checked capacity.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_MEDIA_METADATA_H
#define MULIB_MEDIA_METADATA_H
#include "mulib-media-core.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {const char *key,*value;} mulib_media_metadata_item;
static inline const char *mulib_media_metadata_get(const mulib_media_metadata_item *a,size_t n,const char *key){if(!a||!key)return NULL;for(size_t i=0;i<n;i++)if(a[i].key&&!strcmp(a[i].key,key))return a[i].value;return NULL;}
static inline bool mulib_media_metadata_set(mulib_media_metadata_item *a,size_t *n,size_t cap,const char *key,const char *value){if(!a||!n||*n>cap||!key||!*key||!value)return false;for(size_t i=0;i<*n;i++)if(a[i].key&&!strcmp(a[i].key,key)){a[i].value=value;return true;}if(*n==cap)return false;a[*n].key=key;a[*n].value=value;(*n)++;return true;}
#ifdef __cplusplus
}
#endif
#endif
