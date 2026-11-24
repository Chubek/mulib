/** @file mulib-kf8write.h
 * @brief KF8 FDST flow table construction (building block, not an AZW3 ebook writer).
 * ISO C11, no third-party libraries. Define MULIB_IMPL_KF8WRITE once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-kf8write.md for scope and limitations.
 */
#ifndef MULIB_KF8WRITE_H
#define MULIB_KF8WRITE_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_kf8write_flow { uint32_t offset,length; } mulib_kf8write_flow_t;
int mulib_kf8write_fdst(const mulib_kf8write_flow_t *flows,size_t count,mulib_typecore_buf_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_KF8WRITE_H */
#if defined(MULIB_IMPL_KF8WRITE) && !defined(MULIB_KF8WRITE_H_IMPLEMENTED)
#define MULIB_KF8WRITE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_kf8write_fdst(const mulib_kf8write_flow_t *f,size_t n,mulib_typecore_buf_t *out){if(!out||(!f&&n)||n>(UINT32_MAX-12u)/8u||n>(SIZE_MAX-12u)/8u)return 0;mulib_typecore_buf_t b={0};if(!mulib_typecore_text(&b,"FDST")||!mulib_typecore_u32be(&b,(uint32_t)(12+n*8))||!mulib_typecore_u32be(&b,(uint32_t)n))goto fail;
 uint64_t last=0;for(size_t i=0;i<n;i++){if((uint64_t)f[i].offset<last||(uint64_t)f[i].offset+f[i].length>UINT32_MAX)goto fail;last=(uint64_t)f[i].offset+f[i].length;if(!mulib_typecore_u32be(&b,f[i].offset)||!mulib_typecore_u32be(&b,(uint32_t)last))goto fail;}
 if(!mulib_typecore_append(out,b.data,b.size))goto fail;
 mulib_typecore_free(&b);return 1;
 fail:mulib_typecore_free(&b);return 0;}
#ifdef __cplusplus
}
#endif
#endif
