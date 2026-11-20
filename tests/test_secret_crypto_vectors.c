/* Standalone, native by default. Define MULIB_SECRET_TEST_OPENSSL and link
 * libcrypto for the optional EVP oracle. All keys and messages are test data. */
#include <stdio.h>
#include <string.h>
#include "mulib-secret-crypto-native.h"
#include "mulib-secret-key-agreement.h"
#ifdef MULIB_SECRET_TEST_OPENSSL
#include "mulib-secret-crypto-openssl.h"
#endif

/* Test implementation-after-declaration, with explicit dependency switches. */
#define MULIB_IMPL_SECRET_CORE
#define MULIB_IMPL_SECRET_CRYPTO
#define MULIB_IMPL_SECRET_CRYPTO_NATIVE
#define MULIB_IMPL_SECRET_KEY_AGREEMENT
#define MULIB_IMPL_ENTROPY
#define MULIB_IMPL_AUTHCRYPTO
#define MULIB_IMPL_SHA
#define MULIB_IMPL_HKDF
#define MULIB_IMPL_AES_GCM
#define MULIB_IMPL_X25519
#include "mulib-secret-core.h"
#include "mulib-secret-crypto.h"
#include "mulib-secret-crypto-native.h"
#include "mulib-secret-key-agreement.h"
#ifdef MULIB_SECRET_TEST_OPENSSL
#define MULIB_IMPL_SECRET_CRYPTO_OPENSSL
#include "mulib-secret-crypto-openssl.h"
#endif

#define CHECK(expr) do { \
  if (!(expr)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } \
} while (0)

static mulib_sc_view view(const void *p, size_t n)
{
  mulib_sc_view v = {(const unsigned char *)p, n};
  return v;
}

static mulib_sc_buf buffer(unsigned char *p, size_t n)
{
  mulib_sc_buf b = {p, n};
  return b;
}

static unsigned char from_hex(char c)
{
  return (unsigned char)(c <= '9' ? c - '0' : c - 'a' + 10);
}

static void load_hex(unsigned char *out, size_t n, const char *hex)
{
  for (size_t i = 0; i < n; ++i)
    out[i] = (unsigned char)((from_hex(hex[2 * i]) << 4) | from_hex(hex[2 * i + 1]));
}

static int eq_hex(const unsigned char *out, size_t n, const char *hex)
{
  static const char digits[] = "0123456789abcdef";
  if (strlen(hex) != n * 2) return 0;
  for (size_t i = 0; i < n; ++i)
    if (digits[out[i] >> 4] != hex[2 * i] || digits[out[i] & 15] != hex[2 * i + 1]) return 0;
  return 1;
}

static int all_byte(const unsigned char *p, size_t n, unsigned char byte)
{
  for (size_t i = 0; i < n; ++i) if (p[i] != byte) return 0;
  return 1;
}

static int core_tests(void)
{
  unsigned char bytes[10], reference[8];
  size_t sum = 42;
  memset(bytes, 0xa5, sizeof bytes);
  CHECK(mulib_sc_add(SIZE_MAX, 1, &sum) == MULIB_SC_LIMIT && sum == 42);
  CHECK(mulib_sc_add(SIZE_MAX - 1, 1, &sum) == MULIB_SC_OK && sum == SIZE_MAX);
  CHECK(mulib_sc_overlap(bytes, 5, bytes + 4, 2));
  CHECK(mulib_sc_overlap(bytes + 4, 2, bytes, 5));
  CHECK(!mulib_sc_overlap(bytes, 5, bytes + 5, 2));
  CHECK(!mulib_sc_overlap(bytes, 5, bytes + 4, 0));
  mulib_sc_put64(bytes + 1, UINT64_C(0x0123456789abcdef));
  CHECK(eq_hex(bytes + 1, 8, "0123456789abcdef"));
  CHECK(mulib_sc_get64(bytes + 1) == UINT64_C(0x0123456789abcdef));
  memcpy(reference, bytes + 1, sizeof reference);
  CHECK(mulib_sc_equal(bytes + 1, reference, sizeof reference));
  reference[7] ^= 1;
  CHECK(!mulib_sc_equal(bytes + 1, reference, sizeof reference));
  mulib_sc_wipe(bytes + 1, 8);
  CHECK(bytes[0] == 0xa5 && bytes[9] == 0xa5 && all_byte(bytes + 1, 8, 0));
  return 0;
}

static int digest_tests(const mulib_scr_provider *p)
{
  /* FIPS 180-4 SHA examples; HMAC case 1 of RFC 2202 (SHA-1) and RFC 4231. */
  static const struct { unsigned alg; const char *hash, *hmac; } known[] = {
    {MULIB_SCR_SHA1, "a9993e364706816aba3e25717850c26c9cd0d89d",
     "b617318655057264e28bc0b6fb378c8ef146be00"},
    {MULIB_SCR_SHA256, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
     "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"},
    {MULIB_SCR_SHA384, "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed"
     "8086072ba1e7cc2358baeca134c825a7", "afd03944d84895626b0825f4ab46907f15f9dadbe4101ec6"
     "82aa034c7cebc59cfaea9ea9076ede7f4af152e8b2fa9cb6"},
    {MULIB_SCR_SHA512, "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
     "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",
     "87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cde"
     "daa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854"}
  };
  unsigned char out[65], key[20], ikm[22], salt[16], info[10];
  memset(key, 0x0b, sizeof key);
  for (size_t i = 0; i < sizeof known / sizeof known[0]; ++i) {
    unsigned alg = known[i].alg;
    size_t n = mulib_scr_digest_size(alg);
    memset(out, 0xa5, sizeof out);
    CHECK(mulib_scr_hash(p, alg, view("abc", 3), out, sizeof out) == MULIB_SC_OK);
    CHECK(eq_hex(out, n, known[i].hash) && all_byte(out + n, sizeof out - n, 0xa5));
    CHECK(mulib_scr_hmac(p, alg, view(key, sizeof key), view("Hi There", 8),
                         out, sizeof out) == MULIB_SC_OK);
    CHECK(eq_hex(out, n, known[i].hmac) && all_byte(out + n, sizeof out - n, 0xa5));
  }

  /* RFC 5869 A.1 and A.3, including an omitted salt and info. */
  memset(ikm, 0x0b, sizeof ikm);
  for (size_t i = 0; i < sizeof salt; ++i) salt[i] = (unsigned char)i;
  for (size_t i = 0; i < sizeof info; ++i) info[i] = (unsigned char)(0xf0 + i);
  memset(out, 0xa5, sizeof out);
  CHECK(mulib_scr_hkdf(p, MULIB_SCR_SHA256, view(salt, 13), view(ikm, sizeof ikm),
                       view(info, sizeof info), out, 42) == MULIB_SC_OK);
  CHECK(eq_hex(out, 42, "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865"));
  CHECK(all_byte(out + 42, sizeof out - 42, 0xa5));
  CHECK(mulib_scr_hkdf(p, MULIB_SCR_SHA256, view(NULL, 0), view(ikm, sizeof ikm),
                       view(NULL, 0), out, 42) == MULIB_SC_OK);
  CHECK(eq_hex(out, 42, "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d9d201395faa4b61a96c8"));
  /* Python hashlib.pbkdf2_hmac, using a binary password and the policy floor. */
  mulib_scr_kdf_params params = {MULIB_SCR_PBKDF2_SHA256, 600000, 0, 0};
  memset(out, 0xa5, sizeof out);
  CHECK(mulib_scr_kdf(p, params, view("a\0password", 10), view(salt, sizeof salt), out) == MULIB_SC_OK);
  CHECK(eq_hex(out, 32, "8847859e8edd14cf1323b30576c2fd656bf53ec6d2fb2f0cbc0345edfcde73e9"));
  CHECK(all_byte(out + 32, sizeof out - 32, 0xa5));
  return 0;
}

static int hkdf_limit_tests(const mulib_scr_provider *p)
{
  /* Last blocks independently generated with Python hashlib/HMAC. */
  static const struct { unsigned alg; const char *last; } known[] = {
    {MULIB_SCR_SHA384, "648d60c534618ab2b9eec57d949f4df13fdbb11aea392474970a398cdc2da447"
     "0f0bc05c1789af0eccb248f596263281"},
    {MULIB_SCR_SHA512, "2e6efbf87b77ecc471eea96a7fbeb0001562733600f4fff9015d9351b5a31a744"
     "53d9e09a9a5481dca4aff93d32260498a552dd9ea01bcc1ebd72842bd7e9d17"}
  };
  unsigned char out[255 * 64 + 1];
  mulib_sc_view s = view("secret hkdf test salt", 21);
  mulib_sc_view ikm = view("\0ikm", 4), info = view("test info\0data", 14);
  for (size_t i = 0; i < sizeof known / sizeof known[0]; ++i) {
    size_t d = mulib_scr_digest_size(known[i].alg), n = 255 * d;
    memset(out, 0xa5, sizeof out);
    CHECK(mulib_scr_hkdf(p, known[i].alg, s, ikm, info, out, n + 1) == MULIB_SC_INVALID);
    CHECK(all_byte(out, sizeof out, 0xa5));
    CHECK(mulib_scr_hkdf(p, known[i].alg, s, ikm, info, out, n) == MULIB_SC_OK);
    CHECK(eq_hex(out + n - d, d, known[i].last));
    CHECK(all_byte(out + n, sizeof out - n, 0xa5));
  }
  return 0;
}

static int gcm_vectors(const mulib_scr_provider *p, unsigned alg)
{
  /* NIST GCM zero key/IV vectors with one data block and no AAD. */
  const char *cipher_hex = alg == MULIB_SCR_AES128_GCM ? "0388dace60b6a392f328c2b971b2fe78"
    : "cea7403d4d606b6e074ec5d3baf39d18";
  const char *tag_hex = alg == MULIB_SCR_AES128_GCM ? "ab6e47d42cec13bdf53a67b21257bddf"
    : "d0d1c8a799996bf0265b98b5d48ab919";
  unsigned char zeros[32] = {0}, iv[12] = {0}, out[24], recovered[24], tag[16];
  mulib_sc_view key = view(zeros, mulib_scr_key_size(alg)), nonce = view(iv, sizeof iv);
  mulib_sc_view empty = view(NULL, 0), plain = view(zeros, 16), cipher = view(out, 16);
  memset(out, 0xa5, sizeof out);
  CHECK(mulib_scr_seal(p, alg, key, nonce, empty, plain, buffer(out, sizeof out), tag) == MULIB_SC_OK);
  CHECK(eq_hex(out, 16, cipher_hex) && eq_hex(tag, 16, tag_hex));
  CHECK(all_byte(out + 16, sizeof out - 16, 0xa5));
  memset(recovered, 0xa5, sizeof recovered);
  CHECK(mulib_scr_open(p, alg, key, nonce, empty, cipher, tag,
                       buffer(recovered, sizeof recovered)) == MULIB_SC_OK);
  CHECK(all_byte(recovered, 16, 0) && all_byte(recovered + 16, sizeof recovered - 16, 0xa5));
  memset(recovered, 0xa5, sizeof recovered);
  tag[0] ^= 1;
  CHECK(mulib_scr_open(p, alg, key, nonce, empty, cipher, tag,
                       buffer(recovered, sizeof recovered)) == MULIB_SC_AUTH);
  CHECK(all_byte(recovered, 16, 0) && all_byte(recovered + 16, sizeof recovered - 16, 0xa5));
  iv[0] = 1; /* New nonce for the empty message. */
  CHECK(mulib_scr_seal(p, alg, key, nonce, empty, empty, buffer(NULL, 0), tag) == MULIB_SC_OK);
  CHECK(mulib_scr_open(p, alg, key, nonce, empty, empty, tag, buffer(NULL, 0)) == MULIB_SC_OK);
  tag[0] ^= 1;
  CHECK(mulib_scr_open(p, alg, key, nonce, empty, empty, tag, buffer(NULL, 0)) == MULIB_SC_AUTH);
  return 0;
}

static int fail_rng(void *ctx, unsigned char *out, size_t n)
{
  (void)ctx;
  if (n) out[0] = 0x5a;
  return 1; /* An invalid legacy-boolean result must not escape the ABI. */
}

static int fail_after_expand(void *ctx, unsigned alg, mulib_sc_view key, mulib_sc_view in,
                            unsigned char *out, size_t n)
{
  unsigned *calls = (unsigned *)ctx;
  (void)alg;
  (void)key;
  (void)in;
  memset(out, 0x5a, n);
  return ++*calls == 3 ? MULIB_SC_IO : MULIB_SC_OK;
}

static int validation_tests(const mulib_scr_provider *p)
{
  unsigned char scratch[80], tag[16];
  mulib_sc_view empty = view(NULL, 0), in = view(scratch + 16, 16);
  mulib_sc_view nonce = view(scratch + 32, 12), salt = view(scratch + 48, 16);
  mulib_sc_view key = view(scratch, 16);
  mulib_scr_kdf_params params = {MULIB_SCR_PBKDF2_SHA256, 599999, 0, 0};
  unsigned calls = 0;
  const mulib_scr_provider bad = {
    MULIB_SCR_ABI_V1, &calls, fail_rng, NULL, fail_after_expand, NULL, NULL, NULL
  };
  memset(scratch, 0xa5, sizeof scratch);
  memset(tag, 0xa5, sizeof tag);
  CHECK(mulib_scr_hash(p, MULIB_SCR_SHA256, in, scratch, 32) == MULIB_SC_INVALID);
  CHECK(mulib_scr_hash(p, MULIB_SCR_SHA256, view(NULL, 1), scratch, 32) == MULIB_SC_INVALID);
  CHECK(mulib_scr_hmac(p, MULIB_SCR_SHA512, in, empty, scratch, 63) == MULIB_SC_INVALID);
  CHECK(mulib_scr_hmac(p, MULIB_SCR_SHA256, key, empty, scratch, 32) == MULIB_SC_INVALID);
  CHECK(mulib_scr_hkdf(p, MULIB_SCR_SHA256, empty, empty, in, scratch, 42) == MULIB_SC_INVALID);
  CHECK(mulib_scr_kdf(p, params, in, salt, scratch + 8) == MULIB_SC_INVALID);
  CHECK(mulib_scr_kdf(p, params, view("pw", 2), salt, scratch) == MULIB_SC_LIMIT);
  params.algorithm = UINT32_MAX;
  CHECK(mulib_scr_kdf(p, params, view("pw", 2), salt, scratch) == MULIB_SC_UNSUPPORTED);
  CHECK(mulib_scr_seal(p, MULIB_SCR_AES128_GCM, key, nonce, empty, in,
                       buffer(scratch + 44, 16), scratch + 55) == MULIB_SC_INVALID);
  CHECK(mulib_scr_seal(p, MULIB_SCR_AES128_GCM, key, nonce, empty, in,
                       buffer(scratch + 64, 16), scratch + 20) == MULIB_SC_INVALID);
  CHECK(mulib_scr_open(p, MULIB_SCR_AES128_GCM, key, nonce, empty, in, tag,
                       buffer(scratch + 20, 32)) == MULIB_SC_INVALID);
  CHECK(mulib_scr_open(p, MULIB_SCR_AES128_GCM, key, view(scratch + 32, 11), empty, in, tag,
                       buffer(scratch + 64, 16)) == MULIB_SC_INVALID);
  CHECK(all_byte(scratch, sizeof scratch, 0xa5) && all_byte(tag, sizeof tag, 0xa5));

  mulib_scr_provider unknown_abi = bad;
  unknown_abi.abi = 0;
  CHECK(mulib_scr_random(&unknown_abi, (unsigned char *)&unknown_abi, 4) == MULIB_SC_INVALID);
  CHECK(mulib_scr_random(&unknown_abi, scratch, 5) == MULIB_SC_UNSUPPORTED);
  CHECK(all_byte(scratch, 5, 0) && all_byte(scratch + 5, sizeof scratch - 5, 0xa5));
  memset(scratch, 0xa5, sizeof scratch);
  CHECK(mulib_scr_random(&bad, scratch, 5) == MULIB_SC_IO);
  CHECK(all_byte(scratch, 5, 0) && all_byte(scratch + 5, sizeof scratch - 5, 0xa5));
  CHECK(mulib_ska_generate(&bad, scratch, scratch + 32) == MULIB_SC_IO);
  CHECK(all_byte(scratch, 64, 0) && all_byte(scratch + 64, sizeof scratch - 64, 0xa5));
  memset(scratch, 0xa5, sizeof scratch);
  CHECK(mulib_scr_hkdf(&bad, MULIB_SCR_SHA256, empty, empty, empty, scratch, 65) == MULIB_SC_IO);
  CHECK(calls == 3 && all_byte(scratch, 65, 0) && all_byte(scratch + 65, sizeof scratch - 65, 0xa5));
  memset(scratch, 0xa5, sizeof scratch);
  CHECK(mulib_scr_hash(&bad, MULIB_SCR_SHA256, empty, scratch, sizeof scratch) == MULIB_SC_UNSUPPORTED);
  CHECK(all_byte(scratch, 32, 0) && all_byte(scratch + 32, sizeof scratch - 32, 0xa5));
  CHECK(mulib_scr_seal(p, MULIB_SCR_XCHACHA20_POLY1305, view(scratch, 32),
                       view(scratch + 32, 24), empty, view("test", 4),
                       buffer(scratch + 56, 8), tag) == MULIB_SC_UNSUPPORTED);
  CHECK(all_byte(scratch + 56, 4, 0) && all_byte(scratch + 60, 20, 0xa5));
  CHECK(all_byte(tag, sizeof tag, 0));
  params.algorithm = MULIB_SCR_ARGON2ID;
  params.iterations = 2;
  params.memory_kib = 19456;
  params.lanes = 1;
  memset(scratch, 0xa5, sizeof scratch);
  CHECK(mulib_scr_kdf(p, params, view("pw", 2), salt, scratch) == MULIB_SC_UNSUPPORTED);
  CHECK(all_byte(scratch, 32, 0) && all_byte(scratch + 32, sizeof scratch - 32, 0xa5));
  return 0;
}

static int x25519_tests(const mulib_scr_provider *p)
{
  unsigned char a[32], b[32], ap[32], bp[32], out[32], raw[32], overlap[64];
  unsigned char low[32] = {0};
  mulib_sc_view tr = view("agent1 synthetic transcript", 27);
  /* RFC 7748 section 6.1. */
  load_hex(a, sizeof a, "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a");
  load_hex(b, sizeof b, "5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb");
  CHECK(mulib_x25519_public(ap, a));
  CHECK(eq_hex(ap, 32, "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a"));
  CHECK(mulib_x25519_public(bp, b));
  CHECK(eq_hex(bp, 32, "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f"));
  CHECK(mulib_x25519(raw, a, bp));
  CHECK(eq_hex(raw, 32, "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742"));
  CHECK(mulib_ska_derive(p, a, bp, tr, out) == MULIB_SC_OK);
  CHECK(mulib_ska_derive(p, b, ap, tr, raw) == MULIB_SC_OK);
  CHECK(memcmp(out, raw, sizeof out) == 0);
  /* Python hashlib/HMAC used to derive the expected HKDF output from RFC 7748's raw key. */
  CHECK(eq_hex(out, 32, "e344f1c0a88a1eac16688b700e647e16f5063c610bd0f230d4b5d74a73407324"));
  CHECK(mulib_ska_derive(p, b, ap, view("agent1 synthetic transcript!", 28), raw) == MULIB_SC_OK);
  CHECK(eq_hex(raw, 32, "29a6d9150af0c8e2edf2bdb67b8bb1912ef445164ff8e832772dc8335db25dae"));
  bp[31] ^= 0x80; /* RFC 7748 requires masking this bit on input. */
  CHECK(mulib_ska_derive(p, a, bp, tr, raw) == MULIB_SC_OK && memcmp(out, raw, sizeof out) == 0);
  for (unsigned i = 0; i < 4; ++i) {
    if (i == 1) low[0] = 1;
    if (i == 2) { memset(low, 0xff, sizeof low); low[0] = 0xed; low[31] = 0x7f; }
    if (i == 3) low[0] = 0xee;
    memset(out, 0xa5, sizeof out);
    CHECK(mulib_ska_derive(p, a, low, tr, out) == MULIB_SC_AUTH && all_byte(out, 32, 0));
  }
  memset(overlap, 0xa5, sizeof overlap);
  CHECK(mulib_ska_generate(p, overlap, overlap + 1) == MULIB_SC_INVALID);
  CHECK(mulib_ska_derive(p, overlap, ap, tr, overlap + 1) == MULIB_SC_INVALID);
  CHECK(all_byte(overlap, sizeof overlap, 0xa5));
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
  CHECK(mulib_ska_generate(p, a, ap) == MULIB_SC_OK);
  CHECK(mulib_x25519_public(out, a) && memcmp(out, ap, sizeof out) == 0);
#else
  CHECK(mulib_ska_generate(p, a, ap) == MULIB_SC_IO);
  CHECK(all_byte(a, sizeof a, 0) && all_byte(ap, sizeof ap, 0));
#endif
  return 0;
}

#ifdef MULIB_SECRET_TEST_OPENSSL
static int differential(const mulib_scr_provider *a, const mulib_scr_provider *b)
{
  static const size_t sizes[] = {0, 1, 15, 16, 17, 63, 64, 65, 127, 128, 129, 255};
  unsigned char key[129], nonce[12] = {0}, msg[255], x[255], y[255], tag[16], tag2[16];
  for (size_t i = 0; i < sizeof key; ++i) key[i] = (unsigned char)(0xa5 ^ i);
  for (size_t i = 0; i < sizeof msg; ++i) msg[i] = (unsigned char)(i ^ (i >> 1));
  for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; ++i) {
    mulib_sc_view data = view(sizes[i] ? msg : NULL, sizes[i]);
    mulib_sc_view k = sizes[i] ? view(key, sizeof key) : view(NULL, 0);
    mulib_sc_view ad = view(msg + 100, sizeof msg - 100);
    for (unsigned alg = MULIB_SCR_SHA1; alg <= MULIB_SCR_SHA384; ++alg) {
      size_t n = mulib_scr_digest_size(alg);
      CHECK(mulib_scr_hash(a, alg, data, x, sizeof x) == MULIB_SC_OK);
      CHECK(mulib_scr_hash(b, alg, data, y, sizeof y) == MULIB_SC_OK);
      CHECK(memcmp(x, y, n) == 0);
      CHECK(mulib_scr_hmac(a, alg, k, data, x, sizeof x) == MULIB_SC_OK);
      CHECK(mulib_scr_hmac(b, alg, k, data, y, sizeof y) == MULIB_SC_OK);
      CHECK(memcmp(x, y, n) == 0);
    }
    unsigned alg = MULIB_SCR_AES128_GCM;
    nonce[0] = (unsigned char)i;
    mulib_sc_view iv = view(nonce, sizeof nonce);
    k = view(key, 16);
    CHECK(mulib_scr_seal(a, alg, k, iv, ad, data, buffer(x, sizeof x), tag) == MULIB_SC_OK);
    CHECK(mulib_scr_seal(b, alg, k, iv, ad, data, buffer(y, sizeof y), tag2) == MULIB_SC_OK);
    CHECK(memcmp(x, y, data.len) == 0 && memcmp(tag, tag2, sizeof tag) == 0);
    CHECK(mulib_scr_open(b, alg, k, iv, ad, view(x, data.len), tag,
                         buffer(y, sizeof y)) == MULIB_SC_OK);
    CHECK(memcmp(y, msg, data.len) == 0);
    CHECK(mulib_scr_open(b, alg, k, iv, view(ad.ptr, ad.len - 1), view(x, data.len),
                         tag, buffer(y, sizeof y)) == MULIB_SC_AUTH);
  }
  return 0;
}
#endif

int main(void)
{
  mulib_scr_provider native;
  CHECK(!core_tests());
  mulib_scr_native(&native);
  CHECK(mulib_scr_native_supports(MULIB_SCR_OP_RANDOM, 0));
  CHECK(mulib_scr_native_supports(MULIB_SCR_OP_OPEN, MULIB_SCR_AES128_GCM));
  CHECK(!mulib_scr_native_supports(MULIB_SCR_OP_SEAL, MULIB_SCR_AES256_GCM));
  CHECK(!mulib_scr_native_supports(MULIB_SCR_OP_HKDF, MULIB_SCR_SHA1));
  CHECK(!mulib_scr_native_supports(MULIB_SCR_OP_KDF, MULIB_SCR_ARGON2ID));
  CHECK(!digest_tests(&native) && !hkdf_limit_tests(&native));
  CHECK(!gcm_vectors(&native, MULIB_SCR_AES128_GCM));
  CHECK(!validation_tests(&native) && !x25519_tests(&native));
#ifdef MULIB_SECRET_TEST_OPENSSL
  mulib_scr_provider openssl;
  mulib_scr_openssl(&openssl);
  CHECK(mulib_scr_openssl_supports(MULIB_SCR_OP_OPEN, MULIB_SCR_AES256_GCM));
  CHECK(!mulib_scr_openssl_supports(MULIB_SCR_OP_OPEN, MULIB_SCR_XCHACHA20_POLY1305));
  CHECK(!digest_tests(&openssl) && !hkdf_limit_tests(&openssl));
  CHECK(!gcm_vectors(&openssl, MULIB_SCR_AES128_GCM));
  CHECK(!gcm_vectors(&openssl, MULIB_SCR_AES256_GCM) && !differential(&native, &openssl));
  CHECK(!validation_tests(&openssl));
#endif
  puts("secret crypto vectors: OK");
  return 0;
}
