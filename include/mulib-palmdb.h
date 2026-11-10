/** @file mulib-palmdb.h
 * @brief Palm database container writer for fixed records.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_PALMDB once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-palmdb.md for scope and limitations.
 */
#ifndef MULIB_PALMDB_H
#define MULIB_PALMDB_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_palmdb_record { const void *data;size_t size; } mulib_palmdb_record_t;
int mulib_palmdb_write(const char *name,const char type[4],const char creator[4],const mulib_palmdb_record_t *records,size_t count,mulib_typecore_buf_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PALMDB_H */
#if defined(MULIB_IMPL_PALMDB) && !defined(MULIB_PALMDB_H_IMPLEMENTED)
#define MULIB_PALMDB_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_palmdb_write(const char *name,const char type[4],const char creator[4],const mulib_palmdb_record_t *rec,size_t n,mulib_typecore_buf_t *out){if(!name||!type||!creator||(!rec&&n)||!out||strlen(name)>31||n>65535)return 0;
 uint64_t offset=78+8*(uint64_t)n+2;if(offset>UINT32_MAX)return 0;mulib_typecore_buf_t b={0};unsigned char header[78]={0};memcpy(header,name,strlen(name));memcpy(header+60,type,4);memcpy(header+64,creator,4);header[76]=(unsigned char)(n>>8);header[77]=(unsigned char)n;
 if(!mulib_typecore_append(&b,header,78))goto fail;
 for(size_t i=0;i<n;i++){if((!rec[i].data&&rec[i].size)||rec[i].size>UINT32_MAX-offset)goto fail;if(!mulib_typecore_u32be(&b,(uint32_t)offset)||!mulib_typecore_u32be(&b,(uint32_t)(i+1)))goto fail;offset+=rec[i].size;}
 if(!mulib_typecore_u16be(&b,0))goto fail;
 for(size_t i=0;i<n;i++)if(!mulib_typecore_append(&b,rec[i].data,rec[i].size))goto fail;
 if(!mulib_typecore_append(out,b.data,b.size))goto fail;
 mulib_typecore_free(&b);return 1;
 fail:mulib_typecore_free(&b);return 0;}
#ifdef __cplusplus
}
#endif
#endif
