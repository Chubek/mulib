/* MuLIB: eda-journal. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_JOURNAL in exactly one implementation translation unit.
 * Profile: Length-framed snapshots on caller-owned FILE; not a crash-atomic or concurrent WAL.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_JOURNAL_H
#define MULIB_EDA_JOURNAL_H
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
#include "mulib-num-core.h"
#include "mulib-graph-core.h"
#include "mulib-eda-serialize.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_eda_journal_append(FILE *f,const mulib_eda_design_store *s);
int mulib_eda_journal_read(FILE *f,size_t max_bytes,mulib_eda_design_store *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_JOURNAL) && !defined(MULIB_EDA_JOURNAL_H_IMPLEMENTED)
#define MULIB_EDA_JOURNAL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_journal_append(FILE *f,const mulib_eda_design_store *s){if(!f)return 0;mulib_storage_bytes b={0},h={0};int ok=mulib_eda_serialize(s,&b)&&mulib_storage_write_u64le(&h,b.n)&&fwrite(h.v,1,h.n,f)==h.n&&fwrite(b.v,1,b.n,f)==b.n&&fflush(f)==0;mulib_storage_bytes_free(&b);mulib_storage_bytes_free(&h);return ok;}
int mulib_eda_journal_read(FILE *f,size_t max_bytes,mulib_eda_design_store *out){if(!f||!out||out->v)return 0;unsigned char h[8];if(fread(h,1,8,f)!=8)return 0;size_t pos=0;uint64_t n;if(!mulib_storage_read_u64le(h,8,&pos,&n)||n>max_bytes||n>SIZE_MAX)return 0;unsigned char *p=(unsigned char*)mulib_storage_alloc((size_t)n,1);if(!p)return 0;int ok=fread(p,1,(size_t)n,f)==n&&mulib_eda_deserialize(p,(size_t)n,out);free(p);return ok;}
#ifdef __cplusplus
}
#endif
#endif
