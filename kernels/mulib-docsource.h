/** @file mulib-docsource.h
 * @brief Source-origin registry and opt-in checked DocIR byte-span annotations.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_DOCSOURCE once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-docsource.md for scope and limitations.
 */
#ifndef MULIB_DOCSOURCE_H
#define MULIB_DOCSOURCE_H
#include "mulib-typecore.h"
#include "mulib-docir.h"
#include "mulib-typesrc.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_docsource_entry { char *key; char *value; } mulib_docsource_entry_t;
typedef struct mulib_docsource_table { mulib_docsource_entry_t *entries; size_t count, capacity; } mulib_docsource_table_t;
void mulib_docsource_destroy(mulib_docsource_table_t *table);
int mulib_docsource_set(mulib_docsource_table_t *table, const char *key, const char *value);
const char *mulib_docsource_get(const mulib_docsource_table_t *table, const char *key);
int mulib_docsource_erase(mulib_docsource_table_t *table, const char *key);
int mulib_docsource_bind(mulib_docir_node_t *node,const mulib_typesrc_source_t *source,
                        const char *file,size_t begin,size_t end);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DOCSOURCE_H */
#if defined(MULIB_IMPL_DOCSOURCE) && !defined(MULIB_DOCSOURCE_H_IMPLEMENTED)
#define MULIB_DOCSOURCE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
static char *mulib_docsource_copy_(const char *s){size_t n=strlen(s);if(n==SIZE_MAX)return NULL;char *p=(char*)malloc(n+1);if(p)memcpy(p,s,n+1);return p;}
void mulib_docsource_destroy(mulib_docsource_table_t *t){if(!t)return;for(size_t i=0;i<t->count;i++){free(t->entries[i].key);free(t->entries[i].value);}free(t->entries);memset(t,0,sizeof(*t));}
const char *mulib_docsource_get(const mulib_docsource_table_t *t,const char *key){if(!t||!key)return NULL;for(size_t i=0;i<t->count;i++)if(!strcmp(t->entries[i].key,key))return t->entries[i].value;return NULL;}
int mulib_docsource_set(mulib_docsource_table_t *t,const char *key,const char *value){if(!t||!key||!value||!*key)return 0;char *v=mulib_docsource_copy_(value);if(!v)return 0;
 for(size_t i=0;i<t->count;i++)if(!strcmp(t->entries[i].key,key)){free(t->entries[i].value);t->entries[i].value=v;return 1;}
 char *k=mulib_docsource_copy_(key);if(!k){free(v);return 0;}
 if(t->count==t->capacity){if(t->capacity>SIZE_MAX/sizeof(*t->entries)/2){free(k);free(v);return 0;}size_t c=t->capacity?t->capacity*2:8;void *p=realloc(t->entries,c*sizeof(*t->entries));if(!p){free(k);free(v);return 0;}t->entries=(mulib_docsource_entry_t*)p;t->capacity=c;}
 t->entries[t->count++]=(mulib_docsource_entry_t){k,v};return 1;}
int mulib_docsource_erase(mulib_docsource_table_t *t,const char *key){if(!t||!key)return 0;for(size_t i=0;i<t->count;i++)if(!strcmp(t->entries[i].key,key)){free(t->entries[i].key);free(t->entries[i].value);memmove(t->entries+i,t->entries+i+1,(t->count-i-1)*sizeof(*t->entries));t->count--;return 1;}return 0;}
int mulib_docsource_bind(mulib_docir_node_t *node,const mulib_typesrc_source_t *source,const char *file,size_t begin,size_t end) {
#ifdef MULIB_ENABLE_DOCIR
    size_t line,column;
    if(!source||(!source->data&&source->size)||source->size>64u*1024u*1024u||!file||begin>end||end>source->size||
       !mulib_typesrc_locate(source,begin,&line,&column))return 0;
    return mulib_docir_set_source(node,file,begin,end,line,column);
#else
    (void)node;(void)source;(void)file;(void)begin;(void)end;return 0;
#endif
}
#ifdef __cplusplus
}
#endif
#endif
