/** @file mulib-mobiwrite.h
 * @brief PalmDOC type TEXt/REAd writer with uncompressed text record, via palmdb.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_MOBIWRITE once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-mobiwrite.md for scope and limitations.
 */
#ifndef MULIB_MOBIWRITE_H
#define MULIB_MOBIWRITE_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_mobiwrite_palmdoc(const char *name,const char *utf8,size_t length,mulib_typecore_buf_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MOBIWRITE_H */
#if defined(MULIB_IMPL_MOBIWRITE) && !defined(MULIB_MOBIWRITE_H_IMPLEMENTED)
#define MULIB_MOBIWRITE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_mobiwrite_palmdoc(const char *name,const char *text,size_t len,mulib_typecore_buf_t *out){if(!name||(!text&&len)||!out||len>4096)return 0;mulib_typecore_buf_t b={0};unsigned char head[16]={0};head[1]=1;head[4]=(unsigned char)(len>>24);head[5]=(unsigned char)(len>>16);head[6]=(unsigned char)(len>>8);head[7]=(unsigned char)len;head[9]=1;head[10]=16;head[11]=0;
 /* Standalone PalmDOC writer mirrors palmdb record layout without link dependencies. */
 size_t n=2;unsigned char pdb[78]={0};size_t namelen=strlen(name);if(namelen>31)return 0;memcpy(pdb,name,namelen);memcpy(pdb+60,"TEXtREAd",8);pdb[77]=2;
 if(!mulib_typecore_append(&b,pdb,78)||!mulib_typecore_u32be(&b,96)||!mulib_typecore_u32be(&b,1)||!mulib_typecore_u32be(&b,112)||!mulib_typecore_u32be(&b,2)||!mulib_typecore_u16be(&b,0)||!mulib_typecore_append(&b,head,16)||!mulib_typecore_append(&b,text,len)){mulib_typecore_free(&b);return 0;}
 (void)n;int ok=mulib_typecore_append(out,b.data,b.size);mulib_typecore_free(&b);return ok;}
#ifdef __cplusplus
}
#endif
#endif
