/** @file mulib-auxhash.h
 * @brief Portable noncryptographic byte, integer and rolling hashes.
 * Portable noncryptographic hashes; define MULIB_IMPL_AUXHASH once.
 * NULL data is valid at length zero; other invalid buffers return zero.
 * All bytes are unsigned. Murmur/lookup3 word decoding is explicitly little
 * endian. Integer hashes return the full word, before bucket reduction.
 * DJB2 and DJBX33A are the same additive recurrence; DJBX33X uses XOR.
 * Robin Hood is a probing strategy, not a hash function; these hashes can feed
 * its bucket selection. No function here provides collision resistance.
 */
#ifndef MULIB_AUXHASH_H
#define MULIB_AUXHASH_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t mulib_auxhash_djb2(const void *data,size_t length);
uint32_t mulib_auxhash_djbx33a(const void *data,size_t length);
uint32_t mulib_auxhash_djbx33x(const void *data,size_t length);
uint32_t mulib_auxhash_sdbm(const void *data,size_t length);
uint32_t mulib_auxhash_dek(const void *data,size_t length);
uint32_t mulib_auxhash_elf(const void *data,size_t length);
uint32_t mulib_auxhash_pjw(const void *data,size_t length);
uint32_t mulib_auxhash_rs(const void *data,size_t length);
uint32_t mulib_auxhash_js(const void *data,size_t length);
uint32_t mulib_auxhash_ap(const void *data,size_t length);
uint32_t mulib_auxhash_bkdr(const void *data,size_t length);
uint32_t mulib_auxhash_jenkins(const void *data,size_t length);
uint32_t mulib_auxhash_fnv1_32(const void *data,size_t length);
uint32_t mulib_auxhash_fnv1a_32(const void *data,size_t length);
uint64_t mulib_auxhash_fnv1_64(const void *data,size_t length);
uint64_t mulib_auxhash_fnv1a_64(const void *data,size_t length);
uint32_t mulib_auxhash_knuth32(uint32_t value);
uint64_t mulib_auxhash_knuth64(uint64_t value);
uint32_t mulib_auxhash_lookup3(const void *data,size_t length,uint32_t seed);
uint32_t mulib_auxhash_murmur2(const void *data,size_t length,uint32_t seed);
uint64_t mulib_auxhash_murmur2_64a(const void *data,size_t length,uint64_t seed);
uint32_t mulib_auxhash_murmur3_32(const void *data,size_t length,uint32_t seed);
/* BuzHash has no universal table. This seed fixes a reproducible SplitMix64
 * table; pass the same table to hash and rolling updates. Window size > 0.
 * rolling: rotate(hash,1) XOR rotate(table[outgoing],window) XOR table[incoming]. */
void mulib_auxhash_buzhash_table(uint64_t table[256],uint64_t seed);
uint64_t mulib_auxhash_buzhash(const void *data,size_t length,const uint64_t table[256]);
uint64_t mulib_auxhash_buzhash_roll(uint64_t hash,unsigned char outgoing,
                                  unsigned char incoming,size_t window,
                                  const uint64_t table[256]);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_AUXHASH) && !defined(MULIB_AUXHASH_IMPLEMENTED)
#define MULIB_AUXHASH_IMPLEMENTED
#include "detail/mulib-hash-core.h"
#ifdef __cplusplus
extern "C" {
#endif
static uint32_t mulib_auxhash_simple_(const void *data,size_t n,unsigned kind)
{
  const unsigned char *p=(const unsigned char *)data;
  uint32_t h=kind<=1 ? 5381 : kind==3 ? (uint32_t)n : kind==6 ? 1315423911 : kind==7 ? 0xaaaaaaaa : 0;
  uint32_t a=63689;
  if(!p && n) return 0;
  for(size_t i=0;i<n;++i) {
    uint32_t x=p[i],g;
    switch(kind) {
    case 0: h=h*33+x; break;
    case 1: h=h*33^x; break;
    case 2: h=x+(h<<6)+(h<<16)-h; break;
    case 3: h=mulib_hc_r32(h,27)^x; break;
    case 4: h=(h<<4)+x; g=h&0xf0000000; h^=g>>24; h&=~g; break;
    case 5: h=h*a+x; a*=378551; break;
    case 6: h^=(h<<5)+x+(h>>2); break;
    case 7: h^=(i&1) ? ~((h<<11)+(x^(h>>5))) : ((h<<7)^(x*(h>>3))); break;
    case 8: h=h*131+x; break;
    default: h+=x; h+=h<<10; h^=h>>6; break;
    }
  }
  if(kind==9) { h+=h<<3; h^=h>>11; h+=h<<15; }
  return h;
}
#define MULIB_AUX_SIMPLE_(name,kind) \
uint32_t mulib_auxhash_##name(const void *p,size_t n) { return mulib_auxhash_simple_(p,n,kind); }
MULIB_AUX_SIMPLE_(djb2,0)
MULIB_AUX_SIMPLE_(djbx33a,0)
MULIB_AUX_SIMPLE_(djbx33x,1)
MULIB_AUX_SIMPLE_(sdbm,2)
MULIB_AUX_SIMPLE_(dek,3)
MULIB_AUX_SIMPLE_(elf,4)
MULIB_AUX_SIMPLE_(pjw,4)
MULIB_AUX_SIMPLE_(rs,5)
MULIB_AUX_SIMPLE_(js,6)
MULIB_AUX_SIMPLE_(ap,7)
MULIB_AUX_SIMPLE_(bkdr,8)
MULIB_AUX_SIMPLE_(jenkins,9)
#undef MULIB_AUX_SIMPLE_
#define MULIB_AUX_FNV_(type,name,offset,prime,xorfirst) \
type mulib_auxhash_##name(const void *data,size_t n) { \
  const unsigned char *p=(const unsigned char *)data; type h=offset; \
  if(!p && n) return 0; \
  while(n--) { if(xorfirst) h^=*p++; h*=prime; if(!xorfirst) h^=*p++; } return h; }
MULIB_AUX_FNV_(uint32_t,fnv1_32,UINT32_C(2166136261),UINT32_C(16777619),0)
MULIB_AUX_FNV_(uint32_t,fnv1a_32,UINT32_C(2166136261),UINT32_C(16777619),1)
MULIB_AUX_FNV_(uint64_t,fnv1_64,UINT64_C(14695981039346656037),UINT64_C(1099511628211),0)
MULIB_AUX_FNV_(uint64_t,fnv1a_64,UINT64_C(14695981039346656037),UINT64_C(1099511628211),1)
#undef MULIB_AUX_FNV_
uint32_t mulib_auxhash_knuth32(uint32_t x) { return x*UINT32_C(2654435761); }
uint64_t mulib_auxhash_knuth64(uint64_t x) { return x*UINT64_C(11400714819323198485); }
uint32_t mulib_auxhash_lookup3(const void *data,size_t n,uint32_t seed)
{
  const unsigned char *p=(const unsigned char *)data;
  uint32_t a=UINT32_C(0xdeadbeef)+(uint32_t)n+seed,b=a,c=a;
  if(!p && n) return 0;
  while(n>12) {
    a+=mulib_hc_le32(p); b+=mulib_hc_le32(p+4); c+=mulib_hc_le32(p+8);
    a-=c; a^=mulib_hc_r32(c,28); c+=b;
    b-=a; b^=mulib_hc_r32(a,26); a+=c;
    c-=b; c^=mulib_hc_r32(b,24); b+=a;
    a-=c; a^=mulib_hc_r32(c,16); c+=b;
    b-=a; b^=mulib_hc_r32(a,13); a+=c;
    c-=b; c^=mulib_hc_r32(b,28); b+=a;
    n-=12; p+=12;
  }
  if(!n) return c;
  for(size_t i=0;i<n;++i) {
    uint32_t x=(uint32_t)p[i]<<(8*(i%4));
    if(i<4) a+=x; else if(i<8) b+=x; else c+=x;
  }
  c^=b; c-=mulib_hc_r32(b,18); a^=c; a-=mulib_hc_r32(c,21);
  b^=a; b-=mulib_hc_r32(a,7); c^=b; c-=mulib_hc_r32(b,16);
  a^=c; a-=mulib_hc_r32(c,28); b^=a; b-=mulib_hc_r32(a,18);
  c^=b; c-=mulib_hc_r32(b,8); return c;
}
uint32_t mulib_auxhash_murmur2(const void *data,size_t n,uint32_t seed)
{
  const unsigned char *p=(const unsigned char *)data;
  const uint32_t m=UINT32_C(0x5bd1e995); uint32_t h=seed^(uint32_t)n,t=0;
  if(!p && n) return 0;
  while(n>=4) { uint32_t k=mulib_hc_le32(p); k*=m; k^=k>>24; k*=m; h=h*m^k; p+=4; n-=4; }
  for(size_t i=0;i<n;++i) t|=(uint32_t)p[i]<<(8*i);
  if(n) { h^=t; h*=m; }
  h^=h>>13; h*=m; return h^(h>>15);
}
uint64_t mulib_auxhash_murmur2_64a(const void *data,size_t n,uint64_t seed)
{
  const unsigned char *p=(const unsigned char *)data;
  const uint64_t m=UINT64_C(0xc6a4a7935bd1e995); uint64_t h=seed^((uint64_t)n*m),t=0;
  if(!p && n) return 0;
  while(n>=8) { uint64_t k=mulib_hc_le64(p); k*=m; k^=k>>47; k*=m; h=(h^k)*m; p+=8; n-=8; }
  for(size_t i=0;i<n;++i) t|=(uint64_t)p[i]<<(8*i);
  if(n) { h^=t; h*=m; }
  h^=h>>47; h*=m; return h^(h>>47);
}
uint32_t mulib_auxhash_murmur3_32(const void *data,size_t n,uint32_t seed)
{
  const unsigned char *p=(const unsigned char *)data;
  uint32_t h=seed,k=0; size_t length=n;
  if(!p && n) return 0;
  while(n>=4) {
    k=mulib_hc_le32(p)*UINT32_C(0xcc9e2d51); k=mulib_hc_r32(k,17)*UINT32_C(0x1b873593);
    h^=k; h=mulib_hc_r32(h,19); h=h*5+UINT32_C(0xe6546b64); p+=4; n-=4;
  }
  k=0; for(size_t i=0;i<n;++i) k|=(uint32_t)p[i]<<(8*i);
  if(n) { k*=UINT32_C(0xcc9e2d51); k=mulib_hc_r32(k,17)*UINT32_C(0x1b873593); h^=k; }
  h^=(uint32_t)length; h^=h>>16; h*=UINT32_C(0x85ebca6b); h^=h>>13;
  h*=UINT32_C(0xc2b2ae35); return h^(h>>16);
}
void mulib_auxhash_buzhash_table(uint64_t table[256],uint64_t seed)
{
  if(!table) return;
  for(unsigned i=0;i<256;++i) {
    uint64_t z=(seed+=UINT64_C(0x9e3779b97f4a7c15));
    z=(z^(z>>30))*UINT64_C(0xbf58476d1ce4e5b9);
    z=(z^(z>>27))*UINT64_C(0x94d049bb133111eb); table[i]=z^(z>>31);
  }
}
uint64_t mulib_auxhash_buzhash(const void *data,size_t n,const uint64_t table[256])
{
  const unsigned char *p=(const unsigned char *)data; uint64_t h=0;
  if(!table || (!p && n)) return 0;
  while(n--) h=mulib_hc_r64(h,63)^table[*p++];
  return h;
}
uint64_t mulib_auxhash_buzhash_roll(uint64_t h,unsigned char old,unsigned char next,size_t window,const uint64_t table[256])
{
  if(!table || !window) return 0;
  return mulib_hc_r64(h,63)^mulib_hc_r64(table[old],(unsigned)(64-window%64))^table[next];
}
#ifdef __cplusplus
}
#endif
#endif
