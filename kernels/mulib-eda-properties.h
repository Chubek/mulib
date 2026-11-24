/* MuLIB: eda-properties. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_PROPERTIES in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_PROPERTIES_H
#define MULIB_EDA_PROPERTIES_H
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
enum {MULIB_EDA_PROP_INT,MULIB_EDA_PROP_REAL,MULIB_EDA_PROP_TEXT};
typedef struct {unsigned kind;int64_t integer;double real;const char *text;} mulib_eda_property_value;
typedef struct {uint64_t object;char *key;unsigned kind;int64_t integer;double real;char *text;} mulib_eda_property;
typedef struct {mulib_eda_property *v;size_t n,cap;} mulib_eda_properties;
int mulib_eda_properties_set(mulib_eda_properties *p,uint64_t object,const char *key,mulib_eda_property_value v);
const mulib_eda_property *mulib_eda_properties_get(const mulib_eda_properties *p,uint64_t id,const char *key);
void mulib_eda_properties_free(mulib_eda_properties *p);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_PROPERTIES) && !defined(MULIB_EDA_PROPERTIES_H_IMPLEMENTED)
#define MULIB_EDA_PROPERTIES_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_properties_set(mulib_eda_properties *p,uint64_t object,const char *key,mulib_eda_property_value v){if(!p||!object||!key||!*key||v.kind>MULIB_EDA_PROP_TEXT||(v.kind==MULIB_EDA_PROP_REAL&&!isfinite(v.real))||(v.kind==MULIB_EDA_PROP_TEXT&&!v.text))return 0;char *t=v.kind==MULIB_EDA_PROP_TEXT?mulib_storage_string_dup(v.text):NULL;if(v.kind==MULIB_EDA_PROP_TEXT&&!t)return 0;for(size_t i=0;i<p->n;i++)if(p->v[i].object==object&&!strcmp(p->v[i].key,key)){free(p->v[i].text);p->v[i].kind=v.kind;p->v[i].integer=v.integer;p->v[i].real=v.real;p->v[i].text=t;return 1;}mulib_eda_property r={object,mulib_storage_string_dup(key),v.kind,v.integer,v.real,t};if(!r.key||!mulib_storage_push((void**)&p->v,&p->n,&p->cap,sizeof r,&r)){free(r.key);free(t);return 0;}return 1;}
const mulib_eda_property *mulib_eda_properties_get(const mulib_eda_properties *p,uint64_t id,const char *key){if(p&&key)for(size_t i=0;i<p->n;i++)if(p->v[i].object==id&&!strcmp(p->v[i].key,key))return p->v+i;return NULL;}
void mulib_eda_properties_free(mulib_eda_properties *p){if(p){for(size_t i=0;i<p->n;i++){free(p->v[i].key);free(p->v[i].text);}free(p->v);memset(p,0,sizeof *p);}}
#ifdef __cplusplus
}
#endif
#endif
