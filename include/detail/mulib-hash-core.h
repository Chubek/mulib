/* Internal, allocation-free hash primitives. */
#ifndef MULIB_HASH_CORE_H
#define MULIB_HASH_CORE_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#if !defined(MULIB_HASH_NO_INTRINSICS) && defined(__SSE2__)
#include <emmintrin.h>
#define MULIB_HASH_SSE2 1
#else
#define MULIB_HASH_SSE2 0
#endif
static inline uint32_t mulib_hc_r32(uint32_t x, unsigned n)
{ return (x >> (n & 31)) | (x << ((0u-n) & 31)); }
static inline uint64_t mulib_hc_r64(uint64_t x, unsigned n)
{ return (x >> (n & 63)) | (x << ((0u-n) & 63)); }
static inline uint32_t mulib_hc_le32(const unsigned char *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static inline uint64_t mulib_hc_le64(const unsigned char *p)
{ return mulib_hc_le32(p) | (uint64_t)mulib_hc_le32(p+4)<<32; }
static inline uint32_t mulib_hc_be32(const unsigned char *p)
{ return (uint32_t)p[3] | (uint32_t)p[2]<<8 | (uint32_t)p[1]<<16 | (uint32_t)p[0]<<24; }
static inline uint64_t mulib_hc_be64(const unsigned char *p)
{ return mulib_hc_be32(p+4) | (uint64_t)mulib_hc_be32(p)<<32; }
static inline void mulib_hc_putle(unsigned char *p, uint64_t x, size_t n)
{ for (size_t i=0;i<n;++i) { p[i]=(unsigned char)x; x>>=8; } }
static inline void mulib_hc_putbe(unsigned char *p, uint64_t x, size_t n)
{ for (size_t i=0;i<n;++i) { p[n-1-i]=(unsigned char)x; x>>=8; } }
static inline void mulib_hc_wipe(void *p, size_t n)
{ volatile unsigned char *b=(volatile unsigned char *)p; while(n--) *b++=0; }
#endif
