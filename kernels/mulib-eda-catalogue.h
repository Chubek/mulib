/* MuLIB: eda-catalogue. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_CATALOGUE in exactly one implementation translation unit.
 * Profile: Versioned in-memory catalogue index; not a direct binding to the existing CDD registry.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_CATALOGUE_H
#define MULIB_EDA_CATALOGUE_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {char *id,*kind,*uri;uint64_t version;} mulib_eda_catalogue_entry;
typedef struct {mulib_eda_catalogue_entry *v;size_t n,cap;} mulib_eda_catalogue;
int mulib_eda_catalogue_add(mulib_eda_catalogue *c,const char *id,const char *kind,const char *uri,uint64_t version);
const mulib_eda_catalogue_entry *mulib_eda_catalogue_resolve(const mulib_eda_catalogue *c,const char *id,uint64_t version);
void mulib_eda_catalogue_free(mulib_eda_catalogue *c);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_CATALOGUE) && !defined(MULIB_EDA_CATALOGUE_H_IMPLEMENTED)
#define MULIB_EDA_CATALOGUE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_catalogue_add(mulib_eda_catalogue *c,const char *id,const char *kind,const char *uri,uint64_t version){if(!c||!id||!*id||!kind||!*kind||!uri||!version)return 0;for(size_t i=0;i<c->n;i++)if(c->v[i].version==version&&!strcmp(c->v[i].id,id))return 0;mulib_eda_catalogue_entry e={mulib_storage_string_dup(id),mulib_storage_string_dup(kind),mulib_storage_string_dup(uri),version};if(!e.id||!e.kind||!e.uri||!mulib_storage_push((void**)&c->v,&c->n,&c->cap,sizeof e,&e)){free(e.id);free(e.kind);free(e.uri);return 0;}return 1;}
const mulib_eda_catalogue_entry *mulib_eda_catalogue_resolve(const mulib_eda_catalogue *c,const char *id,uint64_t version){const mulib_eda_catalogue_entry *best=NULL;if(c&&id)for(size_t i=0;i<c->n;i++)if(!strcmp(c->v[i].id,id)&&(!version||version==c->v[i].version)&&(!best||best->version<c->v[i].version))best=c->v+i;return best;}
void mulib_eda_catalogue_free(mulib_eda_catalogue *c){if(c){for(size_t i=0;i<c->n;i++){free(c->v[i].id);free(c->v[i].kind);free(c->v[i].uri);}free(c->v);memset(c,0,sizeof *c);}}
#ifdef __cplusplus
}
#endif
#endif
