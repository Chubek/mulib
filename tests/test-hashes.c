/* Include declarations first to exercise delayed implementation inclusion. */
#include "mulib-sha.h"
#include "mulib-cryptohash.h"
#include "mulib-auxhash.h"
#include "mulib-crc.h"
#include "mulib-checksum.h"
#define MULIB_IMPL_SHA
#define MULIB_IMPL_CRYPTOHASH
#define MULIB_IMPL_AUXHASH
#define MULIB_IMPL_CRC
#define MULIB_IMPL_CHECKSUM
#define MULIB_IMPL_HASH
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_ADLER32
#define MULIB_IMPL_AUTHCRYPTO
#include "mulib-sha.h"
#include "mulib-cryptohash.h"
#include "mulib-auxhash.h"
#include "mulib-crc.h"
#include "mulib-checksum.h"
#include "mulib-hash.h"
#include "mulib-crc32.h"
#include "mulib-adler32.h"
#include "mulib-authcrypto.h"
#include <assert.h>
#include <stdio.h>
#include "fixtures/hash-vectors.h"
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
static unsigned char message[102401];
static unsigned nibble(char c) { return c<='9' ? (unsigned)(c-'0') : (unsigned)(c-'a'+10); }
static void expect_hex(const unsigned char *out,const char *hex)
{
  size_t n=strlen(hex)/2;
  for(size_t i=0;i<n;++i) {
    unsigned expected=(nibble(hex[2*i])<<4)|nibble(hex[2*i+1]);
    if(out[i]!=expected) { fprintf(stderr,"digest mismatch at byte %zu: %02x != %02x\n",i,out[i],expected); assert(0); }
  }
}
static void expect_zero(const void *p,size_t n)
{ const unsigned char *b=(const unsigned char *)p; while(n--) assert(*b++==0); }
static void sha_tests(void)
{
  unsigned char out[256],other[256]; mulib_sha_ctx c;
  assert(mulib_sha0(NULL,0,out)); expect_hex(out,"f96cea198ad1dd5617ac084a3d92c6107708c0ef");
  assert(mulib_sha0("abc",3,out)); expect_hex(out,"0164b8a914cd2a5e74c4f7ff082c4d97f1edf880");
  assert(mulib_sha256("abc",3,out)); expect_hex(out,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  for(size_t i=0;i<COUNT(sha_vectors);++i) {
    mulib_sha_algorithm a=sha_vectors[i].algorithm;
    size_t n=sha_vectors[i].length,on=sha_vectors[i].out_length;
    memset(out,0xa5,sizeof out);
    assert(mulib_sha(a,message+1,n,out,on)); expect_hex(out,sha_vectors[i].hex); assert(out[on]==0xa5);
    assert(mulib_sha_intrinsics(a,message+1,n,other,on)); assert(!memcmp(out,other,on));
    for(unsigned simd=0;simd<2;++simd) {
      assert(simd ? mulib_sha_init_intrinsics(&c,a) : mulib_sha_init(&c,a));
      assert(mulib_sha_update(&c,NULL,0));
      for(size_t j=0;j<n;) {
        size_t k=n-j; if(k>73) k=73;
        assert(mulib_sha_update(&c,message+1+j,k)); j+=k;
      }
      if(a>=MULIB_SHAKE128) {
        assert(mulib_sha_squeeze(&c,other,1)); assert(!mulib_sha_update(&c,"x",1));
        assert(mulib_sha_squeeze(&c,other+1,on-1));
      } else {
        assert(!mulib_sha_final(&c,other,on-1)); assert(mulib_sha_final(&c,other,on));
        expect_zero(&c,sizeof c); assert(!mulib_sha_final(&c,other,on));
      }
      assert(!memcmp(out,other,on));
    }
  }
  assert(mulib_sha_init(&c,MULIB_SHA256)); c.bytes_lo=UINT64_MAX/8;
  assert(!mulib_sha_update(&c,"x",1)); assert(c.bytes_lo==UINT64_MAX/8);
  assert(!mulib_sha_init(NULL,MULIB_SHA256)); assert(!mulib_sha_init(&c,(mulib_sha_algorithm)-1));
  assert(!mulib_sha256(NULL,1,out)); assert(!mulib_sha256("x",1,NULL));
  assert(mulib_shake128(NULL,0,NULL,0));
  /* A million 'a' test from the SHA validation suite. */
  memset(other,'a',sizeof other); assert(mulib_sha_init(&c,MULIB_SHA256));
  for(unsigned i=0;i<3906;++i) assert(mulib_sha_update(&c,other,256));
  assert(mulib_sha_update(&c,other,64)); assert(mulib_sha_final(&c,out,32));
  expect_hex(out,"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}
static void blake2_tests(void)
{
  unsigned char key[64],out[65],other[64]; mulib_blake2_ctx c;
  for(unsigned i=0;i<64;++i) key[i]=(unsigned char)i;
  for(size_t i=0;i<COUNT(blake2_vectors);++i) {
    unsigned wide=blake2_vectors[i].wide;
    size_t n=blake2_vectors[i].length,kn=blake2_vectors[i].key_length,on=blake2_vectors[i].out_length;
    memset(out,0xa5,sizeof out);
    assert(wide ? mulib_blake2b(message+1,n,key,kn,out,on) : mulib_blake2s(message+1,n,key,kn,out,on));
    expect_hex(out,blake2_vectors[i].hex); assert(out[on]==0xa5);
    assert(wide ? mulib_blake2b_intrinsics(message+1,n,key,kn,other,on) : mulib_blake2s_intrinsics(message+1,n,key,kn,other,on));
    assert(!memcmp(out,other,on));
    for(unsigned simd=0;simd<2;++simd) {
      assert(wide ? (simd ? mulib_blake2b_init_intrinsics(&c,on,key,kn) : mulib_blake2b_init(&c,on,key,kn))
                  : (simd ? mulib_blake2s_init_intrinsics(&c,on,key,kn) : mulib_blake2s_init(&c,on,key,kn)));
      assert(mulib_blake2_update(&c,NULL,0));
      for(size_t j=0;j<n;++j) assert(mulib_blake2_update(&c,message+1+j,1));
      assert(!mulib_blake2_final(&c,other,on-1)); assert(mulib_blake2_final(&c,other,on));
      assert(!memcmp(out,other,on)); expect_zero(&c,sizeof c);
      assert(!mulib_blake2_update(&c,"x",1));
    }
  }
  assert(!mulib_blake2s_init(&c,0,NULL,0)); assert(!mulib_blake2b_init(&c,65,NULL,0));
  assert(!mulib_blake2s_init(&c,32,key,33)); assert(!mulib_blake2s_init(&c,32,NULL,1));
  assert(!mulib_blake2b(NULL,1,NULL,0,out,64));
  assert(mulib_blake2b_init(&c,64,NULL,0)); c.counter[0]=UINT64_MAX; c.counter[1]=UINT64_MAX;
  assert(!mulib_blake2_update(&c,"x",1));
  assert(mulib_blake2s_init(&c,32,NULL,0)); c.counter[0]=UINT64_MAX;
  assert(!mulib_blake2_update(&c,"x",1));
}
static void blake3_tests(void)
{
  unsigned char out[132],other[131],slice[90]; mulib_blake3_ctx c;
  static const size_t steps[]={1,63,64,65,1023,1024,1025,4096};
  for(size_t i=0;i<COUNT(blake3_vectors);++i) {
    size_t n=blake3_vectors[i].length; assert(n+1<=sizeof message);
    memset(out,0xa5,sizeof out);
    assert(mulib_blake3(message+1,n,out,131)); expect_hex(out,blake3_vectors[i].hash); assert(out[131]==0xa5);
    assert(mulib_blake3_intrinsics(message+1,n,other,131)); assert(!memcmp(out,other,131));
    assert(mulib_blake3_keyed((const unsigned char *)blake3_key,message+1,n,out,131));
    expect_hex(out,blake3_vectors[i].keyed);
    assert(mulib_blake3_derive_key(blake3_context,strlen(blake3_context),message+1,n,out,131));
    expect_hex(out,blake3_vectors[i].derive);
    for(unsigned mode=0;mode<3;++mode) for(unsigned simd=0;simd<2;++simd) for(size_t t=0;t<COUNT(steps);++t) {
      if(mode==0) assert(simd ? mulib_blake3_init_intrinsics(&c) : mulib_blake3_init(&c));
      else if(mode==1) assert(simd ? mulib_blake3_init_keyed_intrinsics(&c,(const unsigned char *)blake3_key)
                                  : mulib_blake3_init_keyed(&c,(const unsigned char *)blake3_key));
      else assert(simd ? mulib_blake3_init_derive_key_intrinsics(&c,blake3_context,strlen(blake3_context))
                       : mulib_blake3_init_derive_key(&c,blake3_context,strlen(blake3_context)));
      assert(mulib_blake3_update(&c,NULL,0));
      for(size_t j=0;j<n;) { size_t k=n-j; if(k>steps[t]) k=steps[t]; assert(mulib_blake3_update(&c,message+1+j,k)); j+=k; }
      assert(mulib_blake3_final(&c,out,131));
      expect_hex(out,mode==0 ? blake3_vectors[i].hash : mode==1 ? blake3_vectors[i].keyed : blake3_vectors[i].derive);
      assert(mulib_blake3_final_seek(&c,41,slice,90)); assert(!memcmp(out+41,slice,90));
      assert(mulib_blake3_final(&c,other,32)); assert(!memcmp(out,other,32));
      assert(!mulib_blake3_final_seek(&c,UINT64_MAX,other,1));
      mulib_cryptohash_wipe(&c,sizeof c); expect_zero(&c,sizeof c);
    }
  }
  assert(!mulib_blake3_init_keyed(&c,NULL)); assert(!mulib_blake3(NULL,1,out,32));
  assert(mulib_blake3(NULL,0,NULL,0)); assert(mulib_blake3_init(&c)); c.total=UINT64_MAX;
  assert(!mulib_blake3_update(&c,"x",1));
}
static void mac_tests(void)
{
  unsigned char key[64],out[80],other[80],data[1000];
  for(unsigned i=0;i<64;++i) key[i]=(unsigned char)i;
  for(unsigned i=0;i<1000;++i) data[i]=(unsigned char)i;
  for(size_t i=0;i<COUNT(siphash_vectors);++i) {
    assert(mulib_siphash24(key,data,i,out)); expect_hex(out,siphash_vectors[i]);
    assert(mulib_siphash13(key,data,i,out)); expect_hex(out,siphash13_vectors[i]);
  }
  for(size_t i=0;i<COUNT(pmac_vectors);++i) {
    size_t n=pmac_vectors[i].length,kn=pmac_vectors[i].key_length;
    if(n==1000) memset(data,0,sizeof data);
    else for(size_t j=0;j<n;++j) data[j]=(unsigned char)j;
    memset(out,0xa5,sizeof out);
    assert(mulib_pmac(key,kn,data,n,out)); expect_hex(out,pmac_vectors[i].hex); assert(out[16]==0xa5);
    assert(mulib_pmac_intrinsics(key,kn,data,n,other)); assert(!memcmp(out,other,16));
  }
  for(unsigned i=0;i<32;++i) key[i]=(unsigned char)(0x40+i);
  for(unsigned i=0;i<200;++i) data[i]=(unsigned char)i;
  for(size_t i=0;i<COUNT(kmac_vectors);++i) {
    unsigned bits=kmac_vectors[i].bits,xof=kmac_vectors[i].xof;
    size_t n=kmac_vectors[i].length,cn=strlen(kmac_vectors[i].custom),on=strlen(kmac_vectors[i].hex)/2;
    typedef int (*kmac_fn)(const void *,size_t,const void *,size_t,const void *,size_t,void *,size_t);
    kmac_fn scalar=xof ? (bits==128 ? mulib_kmacxof128 : mulib_kmacxof256) : (bits==128 ? mulib_kmac128 : mulib_kmac256);
    kmac_fn simd=xof ? (bits==128 ? mulib_kmacxof128_intrinsics : mulib_kmacxof256_intrinsics)
                     : (bits==128 ? mulib_kmac128_intrinsics : mulib_kmac256_intrinsics);
    memset(out,0xa5,sizeof out);
    assert(scalar(key,32,data,n,kmac_vectors[i].custom,cn,out,on)); expect_hex(out,kmac_vectors[i].hex); assert(out[on]==0xa5);
    assert(simd(key,32,data,n,kmac_vectors[i].custom,cn,other,on)); assert(!memcmp(out,other,on));
  }
  assert(!mulib_pmac(NULL,16,data,1,out)); assert(!mulib_pmac(key,17,data,1,out));
  assert(!mulib_pmac(key,16,NULL,1,out)); assert(!mulib_siphash24(NULL,data,1,out));
  assert(!mulib_kmac128(NULL,1,data,1,NULL,0,out,32));
  assert(!mulib_kmac256(key,32,NULL,1,NULL,0,out,32));
  assert(!mulib_kmacxof256(key,32,data,1,NULL,1,out,32));
}
static void crc_tests(void)
{
  mulib_crc_ctx c; mulib_crc_value out,part;
  for(unsigned i=0;i<MULIB_CRC_PRESET_COUNT;++i) {
    const mulib_crc_params *p=mulib_crc_catalogue+i;
    assert(mulib_crc_find(p->name)==p);
    assert(mulib_crc_compute(p,"123456789",9,&out));
    if(out.lo!=p->check.lo || out.hi!=p->check.hi) fprintf(stderr,"CRC mismatch: %s\n",p->name);
    assert(out.lo==p->check.lo && out.hi==p->check.hi);
    for(size_t split=0;split<=9;++split) {
      assert(mulib_crc_init(&c,p)); assert(mulib_crc_update(&c,"123456789",split));
      assert(mulib_crc_update(&c,&"123456789"[split],9-split)); part=mulib_crc_final(&c);
      assert(out.lo==part.lo && out.hi==part.hi);
    }
  }
  for(size_t i=0;i<COUNT(custom_crc_vectors);++i) {
    const mulib_crc_params *p=custom_crc_vectors+i;
    assert(mulib_crc_compute(p,message+1,257,&out));
    assert(out.lo==p->check.lo && out.hi==p->check.hi);
    assert(mulib_crc_init(&c,p));
    for(size_t j=0;j<257;++j) assert(mulib_crc_update(&c,message+1+j,1));
    part=mulib_crc_final(&c); assert(part.lo==out.lo && part.hi==out.hi);
  }
  for(size_t n=0;n<1024;++n) {
    assert(mulib_crc_32_iso_hdlc(message+1,n)==mulib_crc32(message+1,n));
    assert(mulib_crc_32_iscsi(message+1,n)==mulib_crc32c_intrinsics(0,message+1,n));
    size_t split=n/2;
    uint32_t seed=mulib_crc32c_intrinsics(0,message+1,split);
    assert(mulib_crc32c_intrinsics(seed,message+1+split,n-split)==mulib_crc_32_iscsi(message+1,n));
  }
  assert(!mulib_crc_find("unknown")); assert(!mulib_crc_find(NULL));
  assert(!mulib_crc_compute(mulib_crc_catalogue,NULL,1,&out));
  mulib_crc_params p=mulib_crc_catalogue[0]; p.width=0; assert(!mulib_crc_init(&c,&p));
  p.width=129; assert(!mulib_crc_init(&c,&p)); p.width=1; p.initial.lo=2; assert(!mulib_crc_init(&c,&p));
}
static void checksum_tests(void)
{
  const unsigned char internet[]={0x00,0x01,0xf2,0x03,0xf4,0xf5,0xf6,0xf7};
  assert(mulib_checksum_adler32("Wikipedia",9)==UINT32_C(0x11e60398));
  assert(mulib_fletcher16("abcde",5)==0xc8f0);
  assert(mulib_fletcher32("abcde",5)==UINT32_C(0x4ff029c7));
  assert(mulib_fletcher64("abcde",5)==UINT64_C(0x27c4c6c9c6626364));
  assert(mulib_internet_checksum(internet,sizeof internet)==0x220d);
  assert(mulib_internet_checksum(NULL,0)==65535);
  assert(mulib_luhn_digit("7992739871",10)==3); assert(mulib_luhn_valid("79927398713",11));
  assert(!mulib_luhn_valid("79927398714",11)); assert(mulib_luhn_digit("xyz",3)==-1);
  assert(!mulib_luhn_valid("",0));
  for(unsigned a=0;a<=MULIB_CHECKSUM_XOR8;++a) for(size_t n=0;n<257;++n) {
    mulib_checksum_ctx c,d; uint64_t want;
    assert(mulib_checksum_init(&c,(mulib_checksum_algorithm)a));
    assert(mulib_checksum_update(&c,message+1,n)); want=mulib_checksum_final(&c);
    assert(mulib_checksum_init(&d,(mulib_checksum_algorithm)a));
    for(size_t j=0;j<n;++j) assert(mulib_checksum_update(&d,message+1+j,1));
    assert(mulib_checksum_final(&d)==want); assert(mulib_checksum_update(&d,NULL,0));
    if(a==MULIB_CHECKSUM_ADLER32) assert(want==mulib_adler32(message+1,n));
    assert(!mulib_checksum_update(&d,NULL,1)); assert(mulib_checksum_final(&d)==want);
  }
}
static void aux_tests(void)
{
  for(size_t i=0;i<COUNT(aux_vectors);++i) {
    size_t n=aux_vectors[i].length; uint64_t seed=aux_vectors[i].seed;
    assert(mulib_auxhash_lookup3(message+1,n,(uint32_t)seed)==aux_vectors[i].lookup3);
    assert(mulib_auxhash_murmur2(message+1,n,(uint32_t)seed)==aux_vectors[i].murmur2);
    assert(mulib_auxhash_murmur2_64a(message+1,n,seed)==aux_vectors[i].murmur64);
  }
  assert(mulib_auxhash_fnv1a_32("hello",5)==UINT32_C(0x4f9f2cab));
  assert(mulib_auxhash_fnv1_32("hello",5)==UINT32_C(0xb6fa7167));
  assert(mulib_auxhash_fnv1a_64("hello",5)==UINT64_C(0xa430d84680aabd0b));
  assert(mulib_auxhash_fnv1_64("hello",5)==UINT64_C(0x7b495389bdbdd4c7));
  assert(mulib_auxhash_djb2("hello",5)==UINT32_C(0x0f923099));
  assert(mulib_auxhash_dek("hello",5)==UINT32_C(0x0cb33def));
  assert(mulib_auxhash_lookup3(NULL,0,0)==UINT32_C(0xdeadbeef));
  assert(mulib_auxhash_murmur2("hello",5,0)==UINT32_C(0xe56129cb));
  assert(mulib_auxhash_murmur3_32("hello",5,0)==UINT32_C(0x248bfa47));
  for(size_t n=0;n<1024;++n) {
    assert(mulib_auxhash_fnv1a_64(message+1,n)==mulib_hash_fnv1a(message+1,n));
    assert(mulib_auxhash_djb2(message+1,n)==mulib_auxhash_djbx33a(message+1,n));
  }
  uint64_t table[256]; mulib_auxhash_buzhash_table(table,42);
  const size_t windows[]={1,7,32,63,64,65,128};
  for(size_t j=0;j<COUNT(windows);++j) {
    size_t w=windows[j]; uint64_t h=mulib_auxhash_buzhash(message+1,w,table);
    for(size_t i=0;i<500;++i) {
      h=mulib_auxhash_buzhash_roll(h,message[i+1],message[i+1+w],w,table);
      assert(h==mulib_auxhash_buzhash(message+2+i,w,table));
    }
  }
  assert(!mulib_auxhash_djb2(NULL,1)); assert(!mulib_auxhash_murmur2(NULL,1,0));
  assert(mulib_auxhash_knuth32(1)==UINT32_C(2654435761));
}
int main(void)
{
  for(size_t i=1;i<sizeof message;++i) message[i]=(unsigned char)((i-1)%251);
  sha_tests(); blake2_tests(); blake3_tests(); mac_tests(); crc_tests(); checksum_tests(); aux_tests();
  puts("SHA, cryptographic/auxiliary hashes, 113 CRCs and checksums passed"); return 0;
}
