/** @file mulib-checksum.h
 * @brief Streaming Adler, Fletcher, Internet, BSD, System V, sum and XOR checksums.
 * Streaming Adler-32, Fletcher-16/32/64, Internet, BSD, System V, sums and XOR.
 * Define MULIB_IMPL_CHECKSUM once. No allocation. NULL data allowed at length 0.
 * Status APIs return 1/0. final is a snapshot; it does not consume the context.
 * Fletcher words and Internet words are big endian; partial final words are
 * padded with zero low bytes. Fletcher starts with zero, Adler starts with one.
 * These checksums provide no cryptographic integrity.
 */
#ifndef MULIB_CHECKSUM_H
#define MULIB_CHECKSUM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
  MULIB_CHECKSUM_ADLER32, MULIB_CHECKSUM_FLETCHER16, MULIB_CHECKSUM_FLETCHER32,
  MULIB_CHECKSUM_FLETCHER64, MULIB_CHECKSUM_INTERNET, MULIB_CHECKSUM_BSD16,
  MULIB_CHECKSUM_SYSV16, MULIB_CHECKSUM_SUM8, MULIB_CHECKSUM_SUM16,
  MULIB_CHECKSUM_SUM32, MULIB_CHECKSUM_SUM64, MULIB_CHECKSUM_XOR8
} mulib_checksum_algorithm;
typedef struct {
  uint64_t a,b;
  uint32_t partial;
  unsigned used, active;
  mulib_checksum_algorithm algorithm;
} mulib_checksum_ctx;
int mulib_checksum_init(mulib_checksum_ctx *ctx, mulib_checksum_algorithm algorithm);
int mulib_checksum_update(mulib_checksum_ctx *ctx, const void *data, size_t length);
uint64_t mulib_checksum_final(const mulib_checksum_ctx *ctx);
uint32_t mulib_checksum_adler32_update(uint32_t seed, const void *data, size_t length);
uint32_t mulib_checksum_adler32(const void *data, size_t length);
uint16_t mulib_fletcher16(const void *data, size_t length);
uint32_t mulib_fletcher32(const void *data, size_t length);
uint64_t mulib_fletcher64(const void *data, size_t length);
uint16_t mulib_internet_checksum(const void *data, size_t length);
uint16_t mulib_bsd_checksum(const void *data, size_t length);
uint16_t mulib_sysv_checksum(const void *data, size_t length);
uint8_t mulib_checksum_sum8(const void *data, size_t length);
uint16_t mulib_checksum_sum16(const void *data, size_t length);
uint32_t mulib_checksum_sum32(const void *data, size_t length);
uint64_t mulib_checksum_sum64(const void *data, size_t length);
uint8_t mulib_checksum_xor8(const void *data, size_t length);
/* Decimal ASCII only, nonempty. Returns check digit 0..9, or -1 on invalid.
 * luhn_digit appends a digit; luhn_valid checks an already appended digit. */
int mulib_luhn_digit(const char *digits, size_t length);
int mulib_luhn_valid(const char *digits, size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CHECKSUM) && !defined(MULIB_CHECKSUM_IMPLEMENTED)
#define MULIB_CHECKSUM_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_checksum_init(mulib_checksum_ctx *c,mulib_checksum_algorithm a)
{
  if(!c || (unsigned)a>MULIB_CHECKSUM_XOR8) return 0;
  memset(c,0,sizeof *c); c->algorithm=a; c->active=1;
  c->a=a==MULIB_CHECKSUM_ADLER32 ? 1 : 0; return 1;
}
static void mulib_checksum_word_(mulib_checksum_ctx *c,uint32_t word)
{
  uint64_t mod=c->algorithm==MULIB_CHECKSUM_FLETCHER64 ? UINT64_C(4294967295) : 65535;
  if(c->algorithm==MULIB_CHECKSUM_INTERNET) {
    c->a+=word; c->a=(c->a&65535)+(c->a>>16);
  } else { c->a=(c->a+word)%mod; c->b=(c->b+c->a)%mod; }
}
int mulib_checksum_update(mulib_checksum_ctx *c,const void *data,size_t n)
{
  const unsigned char *p=(const unsigned char *)data;
  if(!c || c->active!=1 || (!p && n)) return 0;
  while(n--) {
    unsigned x=*p++;
    switch(c->algorithm) {
    case MULIB_CHECKSUM_ADLER32: c->a=(c->a+x)%65521; c->b=(c->b+c->a)%65521; break;
    case MULIB_CHECKSUM_FLETCHER16: c->a=(c->a+x)%255; c->b=(c->b+c->a)%255; break;
    case MULIB_CHECKSUM_FLETCHER32: case MULIB_CHECKSUM_FLETCHER64: case MULIB_CHECKSUM_INTERNET:
      c->partial=(c->partial<<8)|x;
      if(++c->used==(c->algorithm==MULIB_CHECKSUM_FLETCHER64 ? 4u : 2u)) {
        mulib_checksum_word_(c,c->partial); c->partial=0; c->used=0;
      }
      break;
    case MULIB_CHECKSUM_BSD16:
      c->a=(((c->a>>1)|((c->a&1)<<15))+x)&65535; break;
    case MULIB_CHECKSUM_XOR8: c->a^=x; break;
    case MULIB_CHECKSUM_SYSV16: c->a=(uint32_t)(c->a+x); break;
    default: c->a+=x; break;
    }
  }
  return 1;
}
uint64_t mulib_checksum_final(const mulib_checksum_ctx *ctx)
{
  mulib_checksum_ctx c;
  if(!ctx || ctx->active!=1) return 0;
  c=*ctx;
  if(c.used) {
    unsigned bytes=c.algorithm==MULIB_CHECKSUM_FLETCHER64 ? 4 : 2;
    mulib_checksum_word_(&c,c.partial<<(8*(bytes-c.used)));
  }
  switch(c.algorithm) {
  case MULIB_CHECKSUM_ADLER32: return (c.b<<16)|c.a;
  case MULIB_CHECKSUM_FLETCHER16: return (c.b<<8)|c.a;
  case MULIB_CHECKSUM_FLETCHER32: return (c.b<<16)|c.a;
  case MULIB_CHECKSUM_FLETCHER64: return (c.b<<32)|c.a;
  case MULIB_CHECKSUM_INTERNET:
    c.a=(c.a&65535)+(c.a>>16); return (~c.a)&65535;
  case MULIB_CHECKSUM_SYSV16:
    c.a=(c.a&65535)+(c.a>>16); return ((c.a&65535)+(c.a>>16))&65535;
  case MULIB_CHECKSUM_SUM8: return c.a&255;
  case MULIB_CHECKSUM_SUM16: return c.a&65535;
  case MULIB_CHECKSUM_SUM32: return c.a&UINT32_MAX;
  default: return c.a;
  }
}
uint32_t mulib_checksum_adler32_update(uint32_t seed,const void *p,size_t n)
{
  mulib_checksum_ctx c;
  if(!p && n) return seed;
  mulib_checksum_init(&c,MULIB_CHECKSUM_ADLER32); c.a=seed&65535; c.b=seed>>16;
  mulib_checksum_update(&c,p,n); return (uint32_t)mulib_checksum_final(&c);
}
#define MULIB_CHECKSUM_ONCE_(type,name,algo) \
  type name(const void *p,size_t n) { mulib_checksum_ctx c; \
    if(!mulib_checksum_init(&c,algo) || !mulib_checksum_update(&c,p,n)) return 0; \
    return (type)mulib_checksum_final(&c); }
MULIB_CHECKSUM_ONCE_(uint32_t,mulib_checksum_adler32,MULIB_CHECKSUM_ADLER32)
MULIB_CHECKSUM_ONCE_(uint16_t,mulib_fletcher16,MULIB_CHECKSUM_FLETCHER16)
MULIB_CHECKSUM_ONCE_(uint32_t,mulib_fletcher32,MULIB_CHECKSUM_FLETCHER32)
MULIB_CHECKSUM_ONCE_(uint64_t,mulib_fletcher64,MULIB_CHECKSUM_FLETCHER64)
MULIB_CHECKSUM_ONCE_(uint16_t,mulib_internet_checksum,MULIB_CHECKSUM_INTERNET)
MULIB_CHECKSUM_ONCE_(uint16_t,mulib_bsd_checksum,MULIB_CHECKSUM_BSD16)
MULIB_CHECKSUM_ONCE_(uint16_t,mulib_sysv_checksum,MULIB_CHECKSUM_SYSV16)
MULIB_CHECKSUM_ONCE_(uint8_t,mulib_checksum_sum8,MULIB_CHECKSUM_SUM8)
MULIB_CHECKSUM_ONCE_(uint16_t,mulib_checksum_sum16,MULIB_CHECKSUM_SUM16)
MULIB_CHECKSUM_ONCE_(uint32_t,mulib_checksum_sum32,MULIB_CHECKSUM_SUM32)
MULIB_CHECKSUM_ONCE_(uint64_t,mulib_checksum_sum64,MULIB_CHECKSUM_SUM64)
MULIB_CHECKSUM_ONCE_(uint8_t,mulib_checksum_xor8,MULIB_CHECKSUM_XOR8)
#undef MULIB_CHECKSUM_ONCE_
static int mulib_luhn_sum_(const char *s,size_t n,unsigned twice)
{
  unsigned sum=0;
  if(!s || !n) return -1;
  while(n) {
    unsigned x=(unsigned char)s[--n]; if(x<'0' || x>'9') return -1;
    x-='0'; if(twice) { x*=2; if(x>9) x-=9; }
    sum=(sum+x)%10; twice^=1;
  }
  return (int)sum;
}
int mulib_luhn_digit(const char *s,size_t n)
{ int sum=mulib_luhn_sum_(s,n,1); return sum<0 ? -1 : (10-sum)%10; }
int mulib_luhn_valid(const char *s,size_t n)
{ return mulib_luhn_sum_(s,n,0)==0; }
#ifdef __cplusplus
}
#endif
#endif
