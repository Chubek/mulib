/** @file mulib-typeconformance.h
 * @brief Owned bounded registry of conformance checks and labels; replace, lookup and remove by name.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_TYPECONFORMANCE once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-typeconformance.md for scope and limitations.
 */
#ifndef MULIB_TYPECONFORMANCE_H
#define MULIB_TYPECONFORMANCE_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_typeconformance_entry { char *key; char *value; } mulib_typeconformance_entry_t;
typedef struct mulib_typeconformance_table { mulib_typeconformance_entry_t *entries; size_t count, capacity; } mulib_typeconformance_table_t;
void mulib_typeconformance_destroy(mulib_typeconformance_table_t *table);
int mulib_typeconformance_set(mulib_typeconformance_table_t *table, const char *key, const char *value);
const char *mulib_typeconformance_get(const mulib_typeconformance_table_t *table, const char *key);
int mulib_typeconformance_erase(mulib_typeconformance_table_t *table, const char *key);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TYPECONFORMANCE_H */
#if defined(MULIB_IMPL_TYPECONFORMANCE) && !defined(MULIB_TYPECONFORMANCE_H_IMPLEMENTED)
#define MULIB_TYPECONFORMANCE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
static char *mulib_typeconformance_copy_(const char *s){size_t n=strlen(s);if(n==SIZE_MAX)return NULL;char *p=(char*)malloc(n+1);if(p)memcpy(p,s,n+1);return p;}
void mulib_typeconformance_destroy(mulib_typeconformance_table_t *t){if(!t)return;for(size_t i=0;i<t->count;i++){free(t->entries[i].key);free(t->entries[i].value);}free(t->entries);memset(t,0,sizeof(*t));}
const char *mulib_typeconformance_get(const mulib_typeconformance_table_t *t,const char *key){if(!t||!key)return NULL;for(size_t i=0;i<t->count;i++)if(!strcmp(t->entries[i].key,key))return t->entries[i].value;return NULL;}
int mulib_typeconformance_set(mulib_typeconformance_table_t *t,const char *key,const char *value){if(!t||!key||!value||!*key)return 0;char *v=mulib_typeconformance_copy_(value);if(!v)return 0;
 for(size_t i=0;i<t->count;i++)if(!strcmp(t->entries[i].key,key)){free(t->entries[i].value);t->entries[i].value=v;return 1;}
 char *k=mulib_typeconformance_copy_(key);if(!k){free(v);return 0;}
 if(t->count==t->capacity){if(t->capacity>SIZE_MAX/sizeof(*t->entries)/2){free(k);free(v);return 0;}size_t c=t->capacity?t->capacity*2:8;void *p=realloc(t->entries,c*sizeof(*t->entries));if(!p){free(k);free(v);return 0;}t->entries=(mulib_typeconformance_entry_t*)p;t->capacity=c;}
 t->entries[t->count++]=(mulib_typeconformance_entry_t){k,v};return 1;}
int mulib_typeconformance_erase(mulib_typeconformance_table_t *t,const char *key){if(!t||!key)return 0;for(size_t i=0;i<t->count;i++)if(!strcmp(t->entries[i].key,key)){free(t->entries[i].key);free(t->entries[i].value);memmove(t->entries+i,t->entries+i+1,(t->count-i-1)*sizeof(*t->entries));t->count--;return 1;}return 0;}
#ifdef __cplusplus
}
#endif
#endif
