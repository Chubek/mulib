/* MuLIB / Amb3r: eda-journal. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_JOURNAL in exactly one implementation translation unit.
 * Profile: Length-framed snapshots on caller-owned FILE; not a crash-atomic or concurrent WAL.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_JOURNAL_H
#define MULIB_EDA_JOURNAL_H
#include "mulib-amb3r-base.h"
#include "mulib-eda-serialize.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_eda_journal_append(FILE *f,const ma_design_store *s);
int mulib_eda_journal_read(FILE *f,size_t max_bytes,ma_design_store *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_JOURNAL) && !defined(MULIB_EDA_JOURNAL_H_IMPLEMENTED)
#define MULIB_EDA_JOURNAL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_journal_append(FILE *f,const ma_design_store *s){if(!f)return 0;ma_bytes b={0},h={0};int ok=mulib_eda_serialize(s,&b)&&ma_write_u64(&h,b.n)&&fwrite(h.v,1,h.n,f)==h.n&&fwrite(b.v,1,b.n,f)==b.n&&fflush(f)==0;ma_bytes_free(&b);ma_bytes_free(&h);return ok;}
int mulib_eda_journal_read(FILE *f,size_t max_bytes,ma_design_store *out){if(!f||!out||out->v)return 0;unsigned char h[8];if(fread(h,1,8,f)!=8)return 0;size_t pos=0;uint64_t n;if(!ma_read_u64(h,8,&pos,&n)||n>max_bytes||n>SIZE_MAX)return 0;unsigned char *p=(unsigned char*)ma_alloc((size_t)n,1);if(!p)return 0;int ok=fread(p,1,(size_t)n,f)==n&&mulib_eda_deserialize(p,(size_t)n,out);free(p);return ok;}
#ifdef __cplusplus
}
#endif
#endif
