/* MuLIB: eda-serialize. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_SERIALIZE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_SERIALIZE_H
#define MULIB_EDA_SERIALIZE_H
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
#include "mulib-geom2-core.h"
#include "mulib-graph-core.h"
#include "mulib-eda-store.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_eda_serialize(const mulib_eda_design_store *s,mulib_storage_bytes *out);
int mulib_eda_deserialize(const unsigned char *p,size_t n,mulib_eda_design_store *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_SERIALIZE) && !defined(MULIB_EDA_SERIALIZE_H_IMPLEMENTED)
#define MULIB_EDA_SERIALIZE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_serialize(const mulib_eda_design_store *s,mulib_storage_bytes *out){if(!s||!out||out->v||sizeof(double)!=8)return 0;mulib_storage_bytes t={0};if(!mulib_storage_bytes_append(&t,"AEDA\1",5)||!mulib_storage_write_u64le(&t,s->revision)||!mulib_storage_write_u64le(&t,s->n))goto fail;for(size_t i=0;i<s->n;i++){mulib_eda_design_record r=s->v[i];uint64_t v,x,y;memcpy(&v,&r.value,8);memcpy(&x,&r.position.x,8);memcpy(&y,&r.position.y,8);if(!mulib_storage_write_u64le(&t,r.id)||!mulib_storage_write_u64le(&t,r.parent)||!mulib_storage_write_u64le(&t,((uint64_t)r.kind<<32)|r.flags)||!mulib_storage_write_u64le(&t,v)||!mulib_storage_write_u64le(&t,x)||!mulib_storage_write_u64le(&t,y))goto fail;}*out=t;return 1;fail:mulib_storage_bytes_free(&t);return 0;}
int mulib_eda_deserialize(const unsigned char *p,size_t n,mulib_eda_design_store *out){if(!p||n<21||memcmp(p,"AEDA\1",5)||!out||out->v||sizeof(double)!=8)return 0;size_t pos=5;uint64_t rev,count;if(!mulib_storage_read_u64le(p,n,&pos,&rev)||!mulib_storage_read_u64le(p,n,&pos,&count)||count!=(n-pos)/48||(n-pos)%48)return 0;mulib_eda_design_store t={0};for(uint64_t i=0;i<count;i++){mulib_eda_design_record r={0};uint64_t f,v,x,y;if(!mulib_storage_read_u64le(p,n,&pos,&r.id)||!mulib_storage_read_u64le(p,n,&pos,&r.parent)||!mulib_storage_read_u64le(p,n,&pos,&f)||!mulib_storage_read_u64le(p,n,&pos,&v)||!mulib_storage_read_u64le(p,n,&pos,&x)||!mulib_storage_read_u64le(p,n,&pos,&y))goto fail;r.kind=(uint32_t)(f>>32);r.flags=(uint32_t)f;memcpy(&r.value,&v,8);memcpy(&r.position.x,&x,8);memcpy(&r.position.y,&y,8);if(!mulib_geom2_point_valid(r.position)||mulib_eda_store_find(&t,r.id)!=MULIB_STORAGE_NONE||!mulib_eda_store_put(&t,r))goto fail;}t.revision=rev;*out=t;return 1;fail:mulib_eda_store_free(&t);return 0;}
#ifdef __cplusplus
}
#endif
#endif
