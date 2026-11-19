/* MuLIB / Amb3r: eda-serialize. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_SERIALIZE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_SERIALIZE_H
#define MULIB_EDA_SERIALIZE_H
#include "mulib-amb3r-base.h"
#include "mulib-eda-store.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_eda_serialize(const ma_design_store *s,ma_bytes *out);
int mulib_eda_deserialize(const unsigned char *p,size_t n,ma_design_store *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_SERIALIZE) && !defined(MULIB_EDA_SERIALIZE_H_IMPLEMENTED)
#define MULIB_EDA_SERIALIZE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_serialize(const ma_design_store *s,ma_bytes *out){if(!s||!out||out->v||sizeof(double)!=8)return 0;ma_bytes t={0};if(!ma_bytes_append(&t,"AEDA\1",5)||!ma_write_u64(&t,s->revision)||!ma_write_u64(&t,s->n))goto fail;for(size_t i=0;i<s->n;i++){ma_design_record r=s->v[i];uint64_t v,x,y;memcpy(&v,&r.value,8);memcpy(&x,&r.position.x,8);memcpy(&y,&r.position.y,8);if(!ma_write_u64(&t,r.id)||!ma_write_u64(&t,r.parent)||!ma_write_u64(&t,((uint64_t)r.kind<<32)|r.flags)||!ma_write_u64(&t,v)||!ma_write_u64(&t,x)||!ma_write_u64(&t,y))goto fail;}*out=t;return 1;fail:ma_bytes_free(&t);return 0;}
int mulib_eda_deserialize(const unsigned char *p,size_t n,ma_design_store *out){if(!p||n<21||memcmp(p,"AEDA\1",5)||!out||out->v||sizeof(double)!=8)return 0;size_t pos=5;uint64_t rev,count;if(!ma_read_u64(p,n,&pos,&rev)||!ma_read_u64(p,n,&pos,&count)||count!=(n-pos)/48||(n-pos)%48)return 0;ma_design_store t={0};for(uint64_t i=0;i<count;i++){ma_design_record r={0};uint64_t f,v,x,y;if(!ma_read_u64(p,n,&pos,&r.id)||!ma_read_u64(p,n,&pos,&r.parent)||!ma_read_u64(p,n,&pos,&f)||!ma_read_u64(p,n,&pos,&v)||!ma_read_u64(p,n,&pos,&x)||!ma_read_u64(p,n,&pos,&y))goto fail;r.kind=(uint32_t)(f>>32);r.flags=(uint32_t)f;memcpy(&r.value,&v,8);memcpy(&r.position.x,&x,8);memcpy(&r.position.y,&y,8);if(!ma_point_valid(r.position)||mulib_eda_store_find(&t,r.id)!=MA_NONE||!mulib_eda_store_put(&t,r))goto fail;}t.revision=rev;*out=t;return 1;fail:mulib_eda_store_free(&t);return 0;}
#ifdef __cplusplus
}
#endif
#endif
