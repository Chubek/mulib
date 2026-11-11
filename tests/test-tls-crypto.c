#define MULIB_IMPL_ENTROPY
#define MULIB_IMPL_SHA
#define MULIB_IMPL_HKDF
#define MULIB_IMPL_X25519
#define MULIB_IMPL_AES_GCM
#define MULIB_IMPL_DER
#include "mulib-hkdf.h"
#include "mulib-x25519.h"
#include "mulib-aes-gcm.h"
#include "mulib-der.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void unhex (const char *s, unsigned char *out)
{
  size_t n = strlen (s);
  assert (!(n & 1));
  for (size_t i = 0; i < n; i += 2) {
    unsigned a = s[i] <= '9' ? (unsigned)(s[i] - '0') : (unsigned)(s[i] - 'a' + 10);
    unsigned b = s[i+1] <= '9' ? (unsigned)(s[i+1] - '0') : (unsigned)(s[i+1] - 'a' + 10);
    assert (a < 16 && b < 16); out[i/2] = (unsigned char)(a * 16 + b);
  }
}
static void x25519 (void)
{
  unsigned char a[32], b[32], ap[32], bp[32], expected[32], result[32], zero[32] = {0};
  unhex ("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a", a);
  unhex ("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb", b);
  unhex ("8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a", expected);
  assert (mulib_x25519_public (ap, a) && !memcmp (ap, expected, 32));
  unhex ("de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f", expected);
  assert (mulib_x25519_public (bp, b) && !memcmp (bp, expected, 32));
  unhex ("4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742", expected);
  assert (mulib_x25519 (result, a, bp) && !memcmp (result, expected, 32));
  assert (mulib_x25519 (result, b, ap) && !memcmp (result, expected, 32));
  assert (!mulib_x25519 (result, a, zero) && !memcmp (result, expected, 32));
  bp[31] |= 128;
  assert (mulib_x25519 (result, a, bp) && !memcmp (result, expected, 32));
  assert (mulib_x25519 (a, a, bp) && !memcmp (a, expected, 32));
  memset(a,0,32);a[0]=9;memcpy(b,a,32);
  for (unsigned i=0;i<1000;++i) {
    memcpy(ap,a,32);assert(mulib_x25519(a,a,b));memcpy(b,ap,32);
  }
  unhex("684cf59ba83309552800ef566f2f4d3c1c3887c49360e3875f2eb94d99532c51",expected);
  assert(!memcmp(a,expected,32));
}
static void hkdf (void)
{
  unsigned char ikm[22], salt[13], info[10], prk[32], okm[42], expected[42];
  memset (ikm, 0x0b, sizeof ikm);
  unhex ("000102030405060708090a0b0c", salt); unhex ("f0f1f2f3f4f5f6f7f8f9", info);
  unhex ("077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5", expected);
  assert (mulib_hkdf_extract (MULIB_SHA256, salt, 13, ikm, 22, prk, 32) && !memcmp (prk, expected, 32));
  unhex ("3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865", expected);
  assert (mulib_hkdf_expand (MULIB_SHA256, prk, 32, info, 10, okm, 42) && !memcmp (okm, expected, 42));
  memset (okm, 0xa5, 42);
  assert (!mulib_hkdf_expand (MULIB_SHA256, prk, 32, info, 10, okm, 8161) && okm[0] == 0xa5);
  assert (!mulib_hkdf_expand (MULIB_SHA1, prk, 32, info, 10, okm, 42));
  {
    unsigned char digest[64], known[64], key[20];memset(key,0x0b,sizeof key);
    unhex("afd03944d84895626b0825f4ab46907f15f9dadbe4101ec682aa034c7cebc59cfaea9ea9076ede7f4af152e8b2fa9cb6",known);
    assert(mulib_hmac(MULIB_SHA384,key,20,"Hi There",8,digest,64)&&!memcmp(digest,known,48));
    unhex("87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cdedaa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854",known);
    assert(mulib_hmac(MULIB_SHA512,key,20,"Hi There",8,digest,64)&&!memcmp(digest,known,64));
  }
}
static void gcm (void)
{
  unsigned char key[16] = {0}, nonce[12] = {0}, plain[64] = {0}, cipher[64], tag[16], expected[64], decoded[64];
  mulib_aes_gcm_t c;
  assert (mulib_aes_gcm_init (&c, key));
  for (unsigned portable = 0; portable < 2; ++portable) {
    if (portable) c.hardware = 0;
    assert (mulib_aes_gcm_seal (&c, nonce, NULL, 0, NULL, 0, NULL, tag));
    unhex ("58e2fccefa7e3061367f1d57a4e7455a", expected); assert (!memcmp (tag, expected, 16));
    assert (mulib_aes_gcm_seal (&c, nonce, NULL, 0, plain, 16, cipher, tag));
    unhex ("0388dace60b6a392f328c2b971b2fe78", expected); assert (!memcmp (cipher, expected, 16));
    unhex ("ab6e47d42cec13bdf53a67b21257bddf", expected); assert (!memcmp (tag, expected, 16));
    memset (decoded, 0xa5, sizeof decoded); tag[0] ^= 1;
    assert (!mulib_aes_gcm_open (&c, nonce, NULL, 0, cipher, 16, tag, decoded) && decoded[0] == 0xa5);
    tag[0] ^= 1;
    assert (mulib_aes_gcm_open (&c, nonce, NULL, 0, cipher, 16, tag, cipher) && !memcmp (cipher, plain, 16));
  }
  /* NIST SP 800-38D example with nonempty AAD and a partial last block. */
  unhex ("feffe9928665731c6d6a8f9467308308", key); unhex ("cafebabefacedbaddecaf888", nonce);
  unhex ("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39", plain);
  {
    unsigned char aad[20]; unhex ("feedfacedeadbeeffeedfacedeadbeefabaddad2", aad);
    assert (mulib_aes_gcm_init (&c, key) && mulib_aes_gcm_seal (&c, nonce, aad, 20, plain, 60, cipher, tag));
    unhex ("42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091", expected);
    assert (!memcmp (cipher, expected, 60));
    unhex ("5bc94fbc3221a5db94fae95ae7121a47", expected); assert (!memcmp (tag, expected, 16));
    assert (mulib_aes_gcm_open (&c, nonce, aad, 20, cipher, 60, tag, decoded) && !memcmp (decoded, plain, 60));
  }
  mulib_aes_gcm_clear (&c); assert (!c.active);
}
static void der (void)
{
  static const unsigned char data[] = {0x30,3,2,1,42};
  static const unsigned char bad[][4] = {{2,0x81,1,1},{2,0x80,0,0},{2,2,0,1},{2,1,0x80,0}};
  mulib_der_cursor_t c = {data, sizeof data, 0};
  mulib_der_value_t v; uint32_t n;
  assert (mulib_der_take (&c, 0x30, &v) && c.offset == sizeof data);
  c = mulib_der_contents (&v); assert (mulib_der_take (&c, 2, &v) && mulib_der_u32 (&v, &n) && n == 42);
  for (unsigned i = 0; i < 4; ++i) {
    c.data = bad[i]; c.length = sizeof bad[i]; c.offset = 0;
    assert (!mulib_der_take (&c, 2, &v) || !mulib_der_u32 (&v, &n));
  }
}
int main (void)
{
  x25519 (); hkdf (); gcm (); der ();
  puts ("TLS crypto: RFC 7748/5869, NIST GCM, authentication failure and DER vectors passed");
  return 0;
}
