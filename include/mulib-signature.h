/** @file mulib-signature.h
 * @brief Public-key verification: RSA PKCS#1 v1.5/PSS and ECDSA P-256/P-384.
 * Define MULIB_IMPL_SIGNATURE, DER, SHA, ENTROPY. No signing/private-key API.
 * Arithmetic is variable-time, exclusively for public verification inputs. */
#ifndef MULIB_SIGNATURE_H
#define MULIB_SIGNATURE_H
#include "mulib-der.h"
#include "mulib-sha.h"
#include "mulib-entropy.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_public_key {
  unsigned kind; /* 1 RSA (2048..4096 bits), 23 P-256, 24 P-384 */
  const unsigned char *key, *exponent;
  size_t key_length, exponent_length;
} mulib_public_key_t;
/** TLS SignatureScheme numbers: 0401/0501/0601 RSA PKCS1 SHA256/384/512,
 * 0804/0805/0806 RSA-PSS with digest-sized salt, 0403/0503 ECDSA.
 * RSA key is unsigned big-endian modulus/exponent, EC key uncompressed SEC1.
 * ECDSA signature is canonical DER. Return 1 valid, 0 otherwise. */
int mulib_signature_verify (const mulib_public_key_t *, unsigned scheme,
    const void *message, size_t length, const void *signature, size_t signature_length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SIGNATURE) && !defined(MULIB_SIGNATURE_IMPLEMENTED)
#define MULIB_SIGNATURE_IMPLEMENTED
#include <string.h>
#define MULIB_SIG_LIMBS_ 128
typedef struct mulib_sig_num_ { uint32_t d[MULIB_SIG_LIMBS_]; } mulib_sig_num_;
typedef struct mulib_sig_mod_ { mulib_sig_num_ p, r2, one; unsigned n; uint32_t inv; } mulib_sig_mod_;
static int mulib_sig_cmp_ (const mulib_sig_num_ *a, const mulib_sig_num_ *b, unsigned n)
{ while (n--) { if (a->d[n] != b->d[n]) return a->d[n] > b->d[n] ? 1 : -1; } return 0; }
static int mulib_sig_zero_ (const mulib_sig_num_ *a, unsigned n)
{ uint32_t x = 0; for (unsigned i = 0; i < n; ++i) x |= a->d[i]; return !x; }
static void mulib_sig_load_ (mulib_sig_num_ *a, const unsigned char *p, size_t n)
{ memset (a, 0, sizeof *a); for (size_t i = 0; i < n; ++i) a->d[i/4] |= (uint32_t)p[n-1-i] << (8*(i%4)); }
static void mulib_sig_store_ (const mulib_sig_num_ *a, unsigned char *p, size_t n)
{ for (size_t i = 0; i < n; ++i) p[n-1-i] = (unsigned char)(a->d[i/4] >> (8*(i%4))); }
static void mulib_sig_sub_ (const mulib_sig_mod_ *m, mulib_sig_num_ *o, const mulib_sig_num_ *a, const mulib_sig_num_ *b)
{
  uint64_t borrow = 0, carry = 0;
  for (unsigned i = 0; i < m->n; ++i) { uint64_t v = (uint64_t)a->d[i] - b->d[i] - borrow; o->d[i] = (uint32_t)v; borrow = v >> 63; }
  if (borrow) for (unsigned i = 0; i < m->n; ++i) { uint64_t v = (uint64_t)o->d[i] + m->p.d[i] + carry; o->d[i] = (uint32_t)v; carry = v >> 32; }
}
static void mulib_sig_add_ (const mulib_sig_mod_ *m, mulib_sig_num_ *o, const mulib_sig_num_ *a, const mulib_sig_num_ *b)
{
  uint64_t carry = 0;
  for (unsigned i = 0; i < m->n; ++i) { uint64_t v = (uint64_t)a->d[i] + b->d[i] + carry; o->d[i] = (uint32_t)v; carry = v >> 32; }
  if (carry || mulib_sig_cmp_ (o, &m->p, m->n) >= 0) {
    uint64_t borrow = 0;
    for (unsigned i = 0; i < m->n; ++i) { uint64_t v = (uint64_t)o->d[i] - m->p.d[i] - borrow; o->d[i] = (uint32_t)v; borrow = v >> 63; }
  }
}
static void mulib_sig_mul_ (const mulib_sig_mod_ *m, mulib_sig_num_ *o, const mulib_sig_num_ *a, const mulib_sig_num_ *b)
{
  uint32_t t[2*MULIB_SIG_LIMBS_+2] = {0}; unsigned n = m->n;
  for (unsigned i = 0; i < n; ++i) {
    uint64_t carry = 0;
    for (unsigned j = 0; j < n; ++j) { uint64_t v = (uint64_t)a->d[i]*b->d[j] + t[i+j] + carry; t[i+j] = (uint32_t)v; carry = v >> 32; }
    t[i+n] = (uint32_t)carry;
  }
  for (unsigned i = 0; i < n; ++i) {
    uint32_t q = t[i]*m->inv; uint64_t carry = 0;
    for (unsigned j = 0; j < n; ++j) { uint64_t v = (uint64_t)q*m->p.d[j] + t[i+j] + carry; t[i+j] = (uint32_t)v; carry = v >> 32; }
    for (unsigned j = i+n; carry; ++j) { uint64_t v = (uint64_t)t[j] + carry; t[j] = (uint32_t)v; carry = v >> 32; }
  }
  for (unsigned i = 0; i < n; ++i) o->d[i] = t[i+n];
  if (t[2*n] || mulib_sig_cmp_ (o, &m->p, n) >= 0) {
    uint64_t borrow = 0;
    for (unsigned i = 0; i < n; ++i) { uint64_t v = (uint64_t)o->d[i] - m->p.d[i] - borrow; o->d[i] = (uint32_t)v; borrow = v >> 63; }
  }
}
static void mulib_sig_modinit_ (mulib_sig_mod_ *m, const unsigned char *p, size_t n)
{
  uint32_t inv = 1; mulib_sig_num_ one = {{1}};
  memset (m, 0, sizeof *m); m->n = (unsigned)((n+3)/4); mulib_sig_load_ (&m->p, p, n);
  for (unsigned i = 0; i < 5; ++i) inv *= 2u - m->p.d[0]*inv;
  m->inv = 0u-inv; m->r2 = one;
  for (unsigned i = 0; i < 64*m->n; ++i) mulib_sig_add_ (m, &m->r2, &m->r2, &m->r2);
  mulib_sig_mul_ (m, &m->one, &one, &m->r2);
}
static void mulib_sig_pow_ (const mulib_sig_mod_ *m, mulib_sig_num_ *o, const mulib_sig_num_ *a, const unsigned char *e, size_t n)
{
  mulib_sig_num_ r = m->one;
  for (size_t i = 0; i < n; ++i) for (int bit = 7; bit >= 0; --bit) {
    mulib_sig_mul_ (m, &r, &r, &r);
    if ((e[i] >> bit) & 1) mulib_sig_mul_ (m, &r, &r, a);
  }
  *o = r;
}
static void mulib_sig_inverse_ (const mulib_sig_mod_ *m, mulib_sig_num_ *o, const mulib_sig_num_ *a)
{
  unsigned char e[512]; mulib_sig_store_ (&m->p, e, m->n*4);
  e[m->n*4-1] -= 2; /* primes used here all have low byte >= 2 */
  mulib_sig_pow_ (m, o, a, e, m->n*4);
}
static void mulib_sig_mgf_ (mulib_sha_algorithm alg, const unsigned char *seed, size_t sn, unsigned char *out, size_t n)
{
  unsigned char hash[64], counter[4]; uint32_t c = 0; size_t hs = mulib_sha_digest_size (alg);
  while (n) {
    mulib_sha_ctx h; size_t take = n < hs ? n : hs;
    for (unsigned i = 0; i < 4; ++i) counter[3-i] = (unsigned char)(c >> (8*i));
    (void)mulib_sha_init (&h, alg); (void)mulib_sha_update (&h, seed, sn); (void)mulib_sha_update (&h, counter, 4);
    (void)mulib_sha_final (&h, hash, sizeof hash); memcpy (out, hash, take); out += take; n -= take; ++c;
  }
}
static int mulib_sig_rsa_ (const mulib_public_key_t *k, unsigned scheme, mulib_sha_algorithm alg,
    const unsigned char *digest, const unsigned char *sig, size_t sn)
{
  mulib_sig_mod_ m; mulib_sig_num_ a, r, one = {{1}};
  unsigned char em[512], mask[512], h[64]; size_t hs = mulib_sha_digest_size (alg), bits, en;
  if (!k->key || !k->exponent || k->key_length < 256 || k->key_length > 512 || !k->key[0]
      || !(k->key[k->key_length-1]&1) || !k->exponent_length || k->exponent_length > 4
      || !k->exponent[0] || !(k->exponent[k->exponent_length-1]&1) || sn != k->key_length) return 0;
  { uint32_t e = 0; for (size_t i = 0; i < k->exponent_length; ++i) e = (e<<8)|k->exponent[i]; if (e < 3) return 0; }
  bits = (k->key_length-1)*8; for (unsigned v = k->key[0]; v; v >>= 1) ++bits;
  if (bits < 2048) return 0;
  mulib_sig_modinit_ (&m, k->key, k->key_length); mulib_sig_load_ (&a, sig, sn);
  if (mulib_sig_cmp_ (&a, &m.p, m.n) >= 0) return 0;
  mulib_sig_mul_ (&m, &a, &a, &m.r2); mulib_sig_pow_ (&m, &r, &a, k->exponent, k->exponent_length);
  mulib_sig_mul_ (&m, &r, &r, &one); mulib_sig_store_ (&r, em, sn);
  if (scheme < 0x0800) {
    static const unsigned char prefix[] = {0x30,0x31,0x30,0x0d,6,9,0x60,0x86,0x48,1,0x65,3,4,2,1,5,0,4,0x20};
    unsigned char der[19]; size_t pad = sn-hs-sizeof der-3;
    memcpy (der, prefix, sizeof der); der[1] = (unsigned char)(hs+17); der[14] = alg == MULIB_SHA256 ? 1 : alg == MULIB_SHA384 ? 2 : 3; der[18] = (unsigned char)hs;
    if (pad < 8 || em[0] || em[1] != 1 || em[2+pad]) return 0;
    for (size_t i = 0; i < pad; ++i) if (em[2+i] != 255) return 0;
    return !memcmp (em+3+pad, der, sizeof der) && mulib_secret_equal (em+sn-hs, digest, hs);
  }
  --bits; en = (bits+7)/8;
  if (en < 2*hs+2 || em[sn-1] != 0xbc) return 0;
  for (size_t i = 0; i < sn-en; ++i) if (em[i]) return 0;
  if (sn != en) memmove (em, em+sn-en, en);
  if (em[0] & (unsigned char)(0xffu << (8-(en*8-bits)))) return 0;
  mulib_sig_mgf_ (alg, em+en-hs-1, hs, mask, en-hs-1);
  for (size_t i = 0; i < en-hs-1; ++i) em[i] ^= mask[i];
  em[0] &= (unsigned char)(0xffu >> (en*8-bits));
  for (size_t i = 0; i < en-2*hs-2; ++i) if (em[i]) return 0;
  if (em[en-2*hs-2] != 1) return 0;
  { mulib_sha_ctx ctx; static const unsigned char zero[8] = {0};
    (void)mulib_sha_init (&ctx, alg); (void)mulib_sha_update (&ctx, zero, 8); (void)mulib_sha_update (&ctx, digest, hs);
    (void)mulib_sha_update (&ctx, em+en-2*hs-1, hs); (void)mulib_sha_final (&ctx, h, sizeof h); }
  return mulib_secret_equal (h, em+en-hs-1, hs);
}
typedef struct mulib_sig_point_ { mulib_sig_num_ x,y,z; } mulib_sig_point_;
static void mulib_sig_double_ (const mulib_sig_mod_ *m, mulib_sig_point_ *o, const mulib_sig_point_ *p)
{
  mulib_sig_num_ d,g,b,a,t,u; mulib_sig_point_ r;
  mulib_sig_mul_ (m,&d,&p->z,&p->z); mulib_sig_mul_ (m,&g,&p->y,&p->y); mulib_sig_mul_ (m,&b,&p->x,&g);
  mulib_sig_sub_ (m,&t,&p->x,&d); mulib_sig_add_ (m,&u,&p->x,&d); mulib_sig_mul_ (m,&a,&t,&u);
  mulib_sig_add_ (m,&t,&a,&a); mulib_sig_add_ (m,&a,&t,&a);
  mulib_sig_mul_ (m,&r.x,&a,&a); mulib_sig_add_ (m,&b,&b,&b); mulib_sig_add_ (m,&b,&b,&b);
  mulib_sig_add_ (m,&t,&b,&b); mulib_sig_sub_ (m,&r.x,&r.x,&t);
  mulib_sig_add_ (m,&t,&p->y,&p->z); mulib_sig_mul_ (m,&r.z,&t,&t); mulib_sig_sub_ (m,&r.z,&r.z,&g); mulib_sig_sub_ (m,&r.z,&r.z,&d);
  mulib_sig_sub_ (m,&t,&b,&r.x); mulib_sig_mul_ (m,&r.y,&a,&t); mulib_sig_mul_ (m,&g,&g,&g);
  for (unsigned i=0;i<3;++i) mulib_sig_add_ (m,&g,&g,&g);
  mulib_sig_sub_ (m,&r.y,&r.y,&g); *o=r;
}
/* Add a public affine point to a Jacobian accumulator. */
static void mulib_sig_pointadd_ (const mulib_sig_mod_ *m, mulib_sig_point_ *o, const mulib_sig_point_ *p, const mulib_sig_point_ *q)
{
  mulib_sig_num_ zz,u,s,h,r,hh,hhh,v,t; mulib_sig_point_ out;
  if (mulib_sig_zero_ (&p->z,m->n)) { *o=*q; return; }
  mulib_sig_mul_ (m,&zz,&p->z,&p->z); mulib_sig_mul_ (m,&u,&q->x,&zz);
  mulib_sig_mul_ (m,&s,&q->y,&p->z); mulib_sig_mul_ (m,&s,&s,&zz);
  mulib_sig_sub_ (m,&h,&u,&p->x); mulib_sig_sub_ (m,&r,&s,&p->y);
  if (mulib_sig_zero_ (&h,m->n)) { if (mulib_sig_zero_ (&r,m->n)) mulib_sig_double_ (m,o,p); else memset (o,0,sizeof *o); return; }
  mulib_sig_mul_ (m,&hh,&h,&h); mulib_sig_mul_ (m,&hhh,&h,&hh); mulib_sig_mul_ (m,&v,&p->x,&hh);
  mulib_sig_mul_ (m,&out.x,&r,&r); mulib_sig_sub_ (m,&out.x,&out.x,&hhh); mulib_sig_add_ (m,&t,&v,&v); mulib_sig_sub_ (m,&out.x,&out.x,&t);
  mulib_sig_sub_ (m,&t,&v,&out.x); mulib_sig_mul_ (m,&out.y,&r,&t); mulib_sig_mul_ (m,&t,&p->y,&hhh); mulib_sig_sub_ (m,&out.y,&out.y,&t);
  mulib_sig_mul_ (m,&out.z,&p->z,&h); *o=out;
}
static void mulib_sig_hex_ (const char *s, unsigned char *p, size_t n)
{ for (size_t i=0;i<n;++i) { unsigned a=(unsigned char)s[2*i], b=(unsigned char)s[2*i+1]; p[i]=(unsigned char)(((a<='9'?a-'0':a-'a'+10)<<4)|(b<='9'?b-'0':b-'a'+10)); } }
static int mulib_sig_ec_ (const mulib_public_key_t *k, const unsigned char *hash, size_t hn, const unsigned char *sig, size_t sn)
{
  static const char *const params[2][5] = {
    {"ffffffff00000001000000000000000000000000ffffffffffffffffffffffff",
     "ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551",
     "5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b",
     "6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296",
     "4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5"},
    {"fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffeffffffff0000000000000000ffffffff",
     "ffffffffffffffffffffffffffffffffffffffffffffffffc7634d81f4372ddf581a0db248b0a77aecec196accc52973",
     "b3312fa7e23ee7e4988e056be3f82d19181d9c6efe8141120314088f5013875ac656398d8a2ed19d2a85c8edd3ec2aef",
     "aa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a385502f25dbf55296c3a545e3872760ab7",
     "3617de4a96262c6f5d9e98bf9292dc29f8f41dbd289a147ce9da3113b5f0b8c00a60b1ce1d7e819d7a431d7c90ea0e5f"}};
  size_t n=k->kind==23?32:48, rn, tn; unsigned curve=k->kind==23?0:1;
  const unsigned char *rp,*sp; unsigned char bytes[48];
  mulib_sig_mod_ m,order; mulib_sig_num_ r,s,u,v,t,b,one={{1}}; mulib_sig_point_ q,g,result;
  mulib_der_cursor_t c={sig,sn,0}; mulib_der_value_t seq,a,z;
  if (!k->key || k->key_length!=1+2*n || k->key[0]!=4 || !mulib_der_take (&c,0x30,&seq) || c.offset!=c.length) return 0;
  c=mulib_der_contents (&seq);
  if (!mulib_der_take (&c,2,&a)||!mulib_der_take (&c,2,&z)||c.offset!=c.length||!mulib_der_unsigned (&a,&rp,&rn)||!mulib_der_unsigned (&z,&sp,&tn)||rn>n||tn>n) return 0;
  mulib_sig_hex_ (params[curve][1],bytes,n); mulib_sig_modinit_ (&order,bytes,n);
  mulib_sig_load_ (&r,rp,rn); mulib_sig_load_ (&s,sp,tn);
  if (mulib_sig_zero_ (&r,order.n)||mulib_sig_zero_ (&s,order.n)||mulib_sig_cmp_ (&r,&order.p,order.n)>=0||mulib_sig_cmp_ (&s,&order.p,order.n)>=0) return 0;
  mulib_sig_mul_ (&order,&s,&s,&order.r2); mulib_sig_inverse_ (&order,&s,&s);
  mulib_sig_load_ (&u,hash,hn<n?hn:n); if (mulib_sig_cmp_ (&u,&order.p,order.n)>=0) mulib_sig_sub_ (&order,&u,&u,&order.p);
  mulib_sig_mul_ (&order,&u,&u,&order.r2); mulib_sig_mul_ (&order,&u,&u,&s); mulib_sig_mul_ (&order,&u,&u,&one);
  mulib_sig_mul_ (&order,&v,&r,&order.r2); mulib_sig_mul_ (&order,&v,&v,&s); mulib_sig_mul_ (&order,&v,&v,&one);
  mulib_sig_hex_ (params[curve][0],bytes,n); mulib_sig_modinit_ (&m,bytes,n);
  mulib_sig_load_ (&q.x,k->key+1,n); mulib_sig_load_ (&q.y,k->key+1+n,n);
  if (mulib_sig_cmp_ (&q.x,&m.p,m.n)>=0||mulib_sig_cmp_ (&q.y,&m.p,m.n)>=0) return 0;
  mulib_sig_mul_ (&m,&q.x,&q.x,&m.r2); mulib_sig_mul_ (&m,&q.y,&q.y,&m.r2); q.z=m.one;
  mulib_sig_hex_ (params[curve][2],bytes,n); mulib_sig_load_ (&b,bytes,n); mulib_sig_mul_ (&m,&b,&b,&m.r2);
  mulib_sig_mul_ (&m,&t,&q.x,&q.x); mulib_sig_mul_ (&m,&t,&t,&q.x);
  for (unsigned i=0;i<3;++i) mulib_sig_sub_ (&m,&t,&t,&q.x);
  mulib_sig_add_ (&m,&t,&t,&b); mulib_sig_mul_ (&m,&s,&q.y,&q.y);
  if (mulib_sig_cmp_ (&t,&s,m.n)) return 0;
  mulib_sig_hex_ (params[curve][3],bytes,n); mulib_sig_load_ (&g.x,bytes,n); mulib_sig_mul_ (&m,&g.x,&g.x,&m.r2);
  mulib_sig_hex_ (params[curve][4],bytes,n); mulib_sig_load_ (&g.y,bytes,n); mulib_sig_mul_ (&m,&g.y,&g.y,&m.r2); g.z=m.one;
  memset (&result,0,sizeof result);
  for (int bit=(int)(n*8)-1;bit>=0;--bit) {
    mulib_sig_double_ (&m,&result,&result);
    if ((u.d[(unsigned)bit/32]>>(bit%32))&1) mulib_sig_pointadd_ (&m,&result,&result,&g);
    if ((v.d[(unsigned)bit/32]>>(bit%32))&1) mulib_sig_pointadd_ (&m,&result,&result,&q);
  }
  if (mulib_sig_zero_ (&result.z,m.n)) return 0;
  mulib_sig_inverse_ (&m,&t,&result.z); mulib_sig_mul_ (&m,&t,&t,&t); mulib_sig_mul_ (&m,&t,&t,&result.x); mulib_sig_mul_ (&m,&t,&t,&one);
  if (mulib_sig_cmp_ (&t,&order.p,order.n)>=0) mulib_sig_sub_ (&order,&t,&t,&order.p);
  return !mulib_sig_cmp_ (&t,&r,order.n);
}
int mulib_signature_verify (const mulib_public_key_t *k, unsigned scheme, const void *msg, size_t n, const void *sig, size_t sn)
{
  unsigned char hash[64]; mulib_sha_algorithm alg;
  if (!k||(!msg&&n)||!sig) return 0;
  switch (scheme) {
    case 0x0401: case 0x0403: case 0x0804: alg=MULIB_SHA256; break;
    case 0x0501: case 0x0503: case 0x0805: alg=MULIB_SHA384; break;
    case 0x0601: case 0x0806: alg=MULIB_SHA512; break;
    default: return 0;
  }
  if (!mulib_sha (alg,msg,n,hash,sizeof hash)) return 0;
  if (scheme==0x0403||scheme==0x0503) {
    if (k->kind!=23&&k->kind!=24) return 0;
    return mulib_sig_ec_ (k,hash,mulib_sha_digest_size (alg),(const unsigned char *)sig,sn);
  }
  return k->kind==1 && mulib_sig_rsa_ (k,scheme,alg,hash,(const unsigned char *)sig,sn);
}
#undef MULIB_SIG_LIMBS_
#endif
