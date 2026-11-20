/* Standalone native test; optionally define MULIB_TEST_SECRET_OPENSSL and link
 * -lcrypto to run the same vectors against the independent EVP adapter. */
#include "mulib-secret-aws.h"
#include "mulib-secret-crypto-native.h"
#include "mulib-secret-jose.h"
#include "mulib-secret-oauth.h"
#include "mulib-secret-otp-uri.h"
#include "mulib-secret-passphrase.h"
#include "mulib-secret-recovery.h"
#include "mulib-secret-sign.h"
#include "mulib-secret-ssh.h"
#include "mulib-secret-webauthn.h"

/* Deliberately enable implementation after declaration inclusion. */
#define MULIB_IMPL_ENTROPY
#define MULIB_IMPL_AUTHCRYPTO
#define MULIB_IMPL_SHA
#define MULIB_IMPL_HKDF
#define MULIB_IMPL_AES_GCM
#define MULIB_IMPL_SECRET_CORE
#define MULIB_IMPL_SECRET_CRYPTO
#define MULIB_IMPL_SECRET_CRYPTO_NATIVE
#define MULIB_IMPL_SECRET_AWS
#define MULIB_IMPL_SECRET_CODEC
#define MULIB_IMPL_SECRET_JOSE
#define MULIB_IMPL_SECRET_OAUTH
#define MULIB_IMPL_SECRET_OTP
#define MULIB_IMPL_SECRET_OTP_URI
#define MULIB_IMPL_SECRET_PASSPHRASE
#define MULIB_IMPL_SECRET_RANDOM
#define MULIB_IMPL_SECRET_RECOVERY
#define MULIB_IMPL_SECRET_SIGN
#define MULIB_IMPL_SECRET_SSH
#define MULIB_IMPL_SECRET_STATE
#define MULIB_IMPL_SECRET_WEBAUTHN
#include "mulib-secret-core.h"
#include "mulib-secret-crypto.h"
#include "mulib-secret-codec.h"
#include "mulib-secret-state.h"
#include "mulib-secret-otp.h"
#include "mulib-secret-random.h"
#include "mulib-secret-aws.h"
#include "mulib-secret-crypto-native.h"
#include "mulib-secret-jose.h"
#include "mulib-secret-oauth.h"
#include "mulib-secret-otp-uri.h"
#include "mulib-secret-passphrase.h"
#include "mulib-secret-recovery.h"
#include "mulib-secret-sign.h"
#include "mulib-secret-ssh.h"
#include "mulib-secret-webauthn.h"
#ifdef MULIB_TEST_SECRET_OPENSSL
#define MULIB_IMPL_SECRET_CRYPTO_OPENSSL
#include "mulib-secret-crypto-openssl.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do {                                                       \
  if (!(expr)) {                                                               \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr);                   \
    exit(EXIT_FAILURE);                                                        \
  }                                                                            \
} while (0)

static mulib_sc_view
text(const char *s)
{
  mulib_sc_view v = {(const unsigned char *)s, strlen(s)};
  return v;
}

static void
codecs(void)
{
  /* RFC 4648 §§9–10, omitting padding per the kernel profile. */
  static const char *const plain[] = {"", "f", "fo", "foo", "foob", "fooba", "foobar"};
  static const char *const b32[] = {"", "MY", "MZXQ", "MZXW6", "MZXW6YQ", "MZXW6YTB",
                                   "MZXW6YTBOI"};
  static const char *const b64[] = {"", "Zg", "Zm8", "Zm9v", "Zm9vYg", "Zm9vYmE",
                                   "Zm9vYmFy"};
  static const char *const hex[] = {"", "66", "666f", "666f6f", "666f6f62",
                                   "666f6f6261", "666f6f626172"};
  const char *const *expected[] = {b64, b32, hex};
  unsigned char bytes[8];
  mulib_sc_buf dest = {bytes, sizeof bytes};
  char encoded[16];
  for (unsigned f = MULIB_SCODE_BASE64URL; f <= MULIB_SCODE_HEX; ++f)
    for (size_t i = 0; i < sizeof plain / sizeof plain[0]; ++i) {
      size_t n = 57, written = 58;
      memset(bytes, 0xa5, sizeof bytes);
      CHECK(!mulib_scode_encode(f, text(plain[i]), encoded, sizeof encoded, &n));
      CHECK(n == strlen(encoded) && !strcmp(encoded, expected[f - 1][i]));
      CHECK(!mulib_scode_decode(f, encoded, n, dest, &written));
      CHECK(written == strlen(plain[i]) && !memcmp(bytes, plain[i], written));
      CHECK(bytes[written] == 0xa5);
    }
  size_t n = 7;
  memset(bytes, 0xa5, sizeof bytes);
  CHECK(mulib_scode_decode(MULIB_SCODE_BASE64URL, "Zh", 2, dest, &n) == MULIB_SC_INVALID);
  CHECK(mulib_scode_decode(MULIB_SCODE_BASE64URL, "Zg==", 4, dest, &n) == MULIB_SC_INVALID);
  CHECK(mulib_scode_decode(MULIB_SCODE_BASE64URL, "Zg\0x", 4, dest, &n) == MULIB_SC_INVALID);
  CHECK(mulib_scode_decode(MULIB_SCODE_BASE64URL, "+w", 2, dest, &n) == MULIB_SC_INVALID);
  CHECK(mulib_scode_decode(MULIB_SCODE_BASE32, "MZ", 2, dest, &n) == MULIB_SC_INVALID);
  CHECK(mulib_scode_decode(MULIB_SCODE_BASE32, "MYA", 3, dest, &n) == MULIB_SC_INVALID);
  CHECK(mulib_scode_decode(MULIB_SCODE_BASE32, "my", 2, dest, &n) == MULIB_SC_INVALID);
  CHECK(mulib_scode_decode(MULIB_SCODE_HEX, "A1", 2, dest, &n) == MULIB_SC_INVALID);
  CHECK(mulib_scode_decode(MULIB_SCODE_HEX, "1", 1, dest, &n) == MULIB_SC_INVALID);
  mulib_sc_buf short_dest = {bytes, 1};
  CHECK(mulib_scode_decode(MULIB_SCODE_HEX, "ffff", 4, short_dest, &n) == MULIB_SC_LIMIT);
  CHECK(mulib_scode_encode(MULIB_SCODE_HEX, text("abc"), (char *)bytes, 6, &n) == MULIB_SC_LIMIT);
  CHECK(n == 7 && bytes[0] == 0xa5 && bytes[1] == 0xa5);
  memcpy(bytes, "Zg", 3);
  CHECK(mulib_scode_decode(MULIB_SCODE_BASE64URL, (char *)bytes, 2, dest, &n) == MULIB_SC_INVALID);
  CHECK(n == 7 && !memcmp(bytes, "Zg", 3));
}

typedef struct {
  mulib_ss_counter stored;
  unsigned calls;
  int fail;
} test_counter;

/* Model of CAS outcomes for this test only; not durable storage. */
static int
counter_cas(void *ctx, const mulib_ss_counter *old, const mulib_ss_counter *next)
{
  test_counter *db = (test_counter *)ctx;
  ++db->calls;
  if (db->fail) return db->fail;
  if (db->stored.revision != old->revision || db->stored.counter != old->counter
      || memcmp(db->stored.id, old->id, sizeof old->id)) return MULIB_SC_CONFLICT;
  db->stored = *next;
  return MULIB_SC_OK;
}

static void
otp(const mulib_scr_provider *p)
{
  /* RFC 4226 Appendix D and RFC 6238 Appendix B. */
  static const char *const hotp[] = {
    "755224", "287082", "359152", "969429", "338314",
    "254676", "287922", "162583", "399871", "520489"
  };
  static const uint64_t times[] = {59, 1111111109, 1111111111, 1234567890,
                                  2000000000, UINT64_C(20000000000)};
  static const char *const expected[][3] = {
    {"94287082", "46119246", "90693936"}, {"07081804", "68084774", "25091201"},
    {"14050471", "67062674", "99943326"}, {"89005924", "91819424", "93441116"},
    {"69279037", "90698825", "38618901"}, {"65353130", "77737706", "47863826"}
  };
  static const char *const seeds[] = {
    "12345678901234567890", "12345678901234567890123456789012",
    "1234567890123456789012345678901234567890123456789012345678901234"
  };
  const unsigned alg[] = {MULIB_SCR_SHA1, MULIB_SCR_SHA256, MULIB_SCR_SHA512};
  char code[9];
  for (unsigned i = 0; i < sizeof hotp / sizeof hotp[0]; ++i) {
    CHECK(!mulib_sotp_hotp(p, alg[0], text(seeds[0]), i, 6, code, sizeof code));
    CHECK(!strcmp(code, hotp[i]));
  }
  for (unsigned i = 0; i < sizeof times / sizeof times[0]; ++i)
    for (unsigned j = 0; j < 3; ++j) {
      uint32_t remaining = 0;
      CHECK(!mulib_sotp_totp(p, alg[j], text(seeds[j]), times[i], 0, 30, 8, code,
                            sizeof code, &remaining));
      CHECK(!strcmp(code, expected[i][j]) && remaining == 30 - times[i] % 30);
    }
  /* Independent Python HMAC + Steam alphabet calculation. */
  CHECK(!mulib_sotp_steam(p, text(seeds[0]), 59, code) && !strcmp(code, "PV9M4"));
  CHECK(mulib_sotp_totp(p, alg[0], text(seeds[0]), 10, 11, 30, 8, code, sizeof code,
                        NULL) == MULIB_SC_INVALID);
  CHECK(!strcmp(code, "PV9M4"));

  mulib_ss_counter state = {1, 0, {1}};
  test_counter db = {state, 0, 0};
  CHECK(!mulib_sotp_totp_verify(p, alg[2], text(seeds[2]), 60, 0, 30, 8,
                               text(expected[0][2]), 1, 0, &state, counter_cas, &db));
  CHECK(state.counter == 2 && state.revision == 2 && db.calls == 1);
  CHECK(mulib_sotp_totp_verify(p, alg[2], text(seeds[2]), 60, 0, 30, 8,
                              text(expected[0][2]), 1, 0, &state, counter_cas, &db)
        == MULIB_SC_AUTH && db.calls == 1);
  CHECK(mulib_sotp_totp_verify(p, alg[2], text(seeds[2]), 59, 0, 30, 8,
                              text(expected[0][2]), 11, 0, &state, counter_cas, &db)
        == MULIB_SC_INVALID && db.calls == 1);
  CHECK(mulib_sotp_totp_verify(p, alg[2], text(seeds[2]), 59, 0, 30, 8,
                              text("9069393x"), 0, 0, &state, counter_cas, &db)
        == MULIB_SC_INVALID && db.calls == 1);
  /* Python HMAC confirms a 6-digit collision at steps 2386 and 2394. */
  CHECK(mulib_sotp_totp_verify(p, alg[0], text(seeds[0]), 2394 * 30, 0, 30, 6,
                              text("709847"), 8, 0, &state, counter_cas, &db)
        == MULIB_SC_AUTH && db.calls == 1);

  db.fail = MULIB_SC_UNCERTAIN;
  strcpy(code, "sentinel");
  CHECK(mulib_sotp_next(p, alg[0], text(seeds[0]), 6, &state, counter_cas, &db,
                       code, sizeof code) == MULIB_SC_UNCERTAIN);
  CHECK(state.revision == 0 && !strcmp(code, "sentinel") && db.calls == 2);
  CHECK(mulib_sotp_next(p, alg[0], text(seeds[0]), 6, &state, counter_cas, &db,
                       code, sizeof code) == MULIB_SC_STATE && db.calls == 2);

  state = db.stored;
  db.fail = 0;
  CHECK(!mulib_sotp_hotp(p, alg[1], text(seeds[1]), UINT64_MAX, 8, code, sizeof code));
  CHECK(mulib_sotp_totp_verify(p, alg[1], text(seeds[1]), UINT64_MAX, 0, 1, 8,
                              text(code), 0, 10, &state, counter_cas, &db)
        == MULIB_SC_LIMIT && db.calls == 2);
}

static void
uris(void)
{
  const char *valid = "otpauth://hotp/%C3%85%3A%20alice%40host?"
                      "secret=jbswy3dpehpk3pxpaa%3d%3d%3d%3d%3d%3d&issuer=%C3%85"
                      "&algorithm=SHA512&counter=18446744073709551615&digits=8";
  mulib_sotp_uri uri;
  CHECK(!mulib_sotp_uri_parse(valid, strlen(valid), &uri));
  CHECK(uri.key_length == 11 && !strcmp(uri.issuer, "\xc3\x85")
        && !strcmp(uri.label, "\xc3\x85: alice@host") && uri.hash == MULIB_SCR_SHA512
        && uri.digits == 8 && uri.hotp && uri.counter == UINT64_MAX);
  mulib_sotp_uri_clear(&uri);
  const char *const malformed[] = {
    "otpauth://totp/Bad%3AAlice?secret=JBSWY3DPEHPK3PXP&issuer=Other",
    "otpauth://totp/A:B:C?secret=JBSWY3DPEHPK3PXP",
    "otpauth://totp/A:%20?secret=JBSWY3DPEHPK3PXP",
    "otpauth://totp/A?secret=JBSWY3DPEHPK3PXP&issuer=A:B",
    "otpauth://totp/a/b?secret=JBSWY3DPEHPK3PXP",
    "otpauth://totp/a b?secret=JBSWY3DPEHPK3PXP",
    "otpauth://totp/%E2%80?secret=JBSWY3DPEHPK3PXP",
    "otpauth://totp/%C0%AF?secret=JBSWY3DPEHPK3PXP",
    "otpauth://totp/%00?secret=JBSWY3DPEHPK3PXP",
    "otpauth://totp/a%?secret=JBSWY3DPEHPK3PXP",
    "otpauth://totp/a?secret=JBSWY3DPEHPK3PXP&secr%65t=JBSWY3DPEHPK3PXP",
    "otpauth://totp/a?secret=JBSWY3DPEHPK3PXP&x=1",
    "otpauth://totp/a?secret=JBSWY3DPEHPK3PXP&",
    "otpauth://totp/a?secret=JBSWY3DPEHPK3PXP#fragment",
    "otpauth://totp/a?secret=JBSWY3DPEHPK3PXPAB======",
    "otpauth://totp/a?secret=JBSWY3DPEHPK3PXPAA=====",
    "otpauth://totp/a?secret=JBSWY3DPEHPK3PXP&period=86401",
    "otpauth://hotp/a?secret=JBSWY3DPEHPK3PXP",
    "otpauth://hotp/a?secret=JBSWY3DPEHPK3PXP&counter=18446744073709551616"
  };
  for (unsigned i = 0; i < sizeof malformed / sizeof malformed[0]; ++i) {
    CHECK(mulib_sotp_uri_parse(malformed[i], strlen(malformed[i]), &uri) == MULIB_SC_INVALID);
    CHECK(!uri.key_length && !uri.key[0]);
  }
}

static int
rng_fail(void *ctx, unsigned char *out, size_t n)
{
  unsigned *calls = (unsigned *)ctx;
  ++*calls;
  if (*calls == 1) {
    memset(out, 0, n);
    return MULIB_SC_OK;
  }
  memset(out, 0xa5, n);
  return MULIB_SC_IO;
}

static int
rng_reject(void *ctx, unsigned char *out, size_t n)
{
  (void)ctx;
  memset(out, 0xff, n);
  return MULIB_SC_OK;
}

static void
generators(const mulib_scr_provider *p)
{
  const unsigned all = MULIB_SRAND_LOWER | MULIB_SRAND_UPPER | MULIB_SRAND_DIGIT
                     | MULIB_SRAND_SYMBOL;
  char out[80];
  CHECK(!mulib_srand_password(p, all, 64, out, sizeof out));
  CHECK(strlen(out) == 64 && strpbrk(out, "abcdefghijklmnopqrstuvwxyz")
        && strpbrk(out, "ABCDEFGHIJKLMNOPQRSTUVWXYZ") && strpbrk(out, "0123456789")
        && strpbrk(out, "!@#$%^&*()-_=+[]"));
  CHECK(mulib_srand_password(p, all, 3, out, sizeof out) == MULIB_SC_INVALID);
  CHECK(mulib_srand_text(p, "abac", 10, out, sizeof out) == MULIB_SC_INVALID);
  unsigned char id[16];
  CHECK(!mulib_srand_uuid(p, id) && (id[6] >> 4) == 4 && (id[8] >> 6) == 2);

  /* Neither malformed wordlists nor insufficient capacity consume entropy. */
  char names[1024][6];
  const char *words[1024];
  for (unsigned i = 0; i < 1024; ++i) {
    CHECK(snprintf(names[i], sizeof names[i], "w%04u", i) == 5);
    words[i] = names[i];
  }
  CHECK(!mulib_sphrase_generate(p, words, 1024, 4, '-', out, sizeof out));
  CHECK(strlen(out) == 23 && out[5] == '-' && out[11] == '-' && out[17] == '-');

  unsigned calls = 0;
  mulib_scr_provider failing = *p;
  failing.ctx = &calls;
  failing.random = rng_fail;
  CHECK(mulib_sphrase_generate(&failing, words, 1024, 4, '-', out, 23) == MULIB_SC_LIMIT);
  words[1023] = words[0];
  CHECK(mulib_sphrase_generate(&failing, words, 1024, 4, '-', out, sizeof out)
        == MULIB_SC_INVALID && !calls);
  words[1023] = "no-word";
  CHECK(mulib_sphrase_generate(&failing, words, 1024, 4, '-', out, sizeof out)
        == MULIB_SC_INVALID && !calls);
  words[1023] = names[1023];
  memset(out, 0x5a, sizeof out);
  CHECK(mulib_sphrase_generate(&failing, words, 1024, 4, '-', out, sizeof out)
        == MULIB_SC_IO && calls == 2);
  CHECK(!memcmp(out, "\0\0\0\0\0\0", 6) && out[6] == 0x5a);

  calls = 0;
  memset(out, 0x5a, sizeof out);
  CHECK(mulib_srand_text(&failing, "ABC", 70, out, sizeof out) == MULIB_SC_IO);
  for (size_t i = 0; i <= 70; ++i) CHECK(!out[i]);
  CHECK(out[71] == 0x5a);
  failing.random = rng_reject;
  CHECK(mulib_srand_text(&failing, "ABC", 70, out, sizeof out) == MULIB_SC_IO);
}

static void
protocol_primitives(const mulib_scr_provider *p)
{
  unsigned char key[32], digest[32], decoded[8];
  for (unsigned i = 0; i < sizeof key; ++i) key[i] = (unsigned char)i;
  mulib_sc_view k = {key, sizeof key};
  char encoded[128];
  size_t n;
  CHECK(!mulib_sphrase_recovery(p, encoded) && strlen(encoded) == 20);
  CHECK(!mulib_sphrase_verifier(p, k, "ABCDEFGHIJKLMNOPQRST", digest));
  mulib_sc_view mac = {digest, sizeof digest};
  CHECK(!mulib_scode_encode(MULIB_SCODE_HEX, mac, encoded, sizeof encoded, &n));
  CHECK(!strcmp(encoded, "c94ad0b25471faff939aa255d87e12049758686d659a63a4edf11bd27b9f6405"));
  CHECK(mulib_sphrase_verifier(p, k, "ABCDEFGHIJKLMNOPQRS!", digest) == MULIB_SC_INVALID);

  /* RFC 7636 Appendix B. */
  const char *verifier = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
  CHECK(!mulib_soauth_pkce_challenge(p, text(verifier), encoded));
  CHECK(!strcmp(encoded, "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM"));
  char generated[44], challenge[44];
  CHECK(!mulib_soauth_pkce(p, generated, challenge));
  CHECK(!mulib_soauth_pkce_challenge(p, text(generated), encoded));
  CHECK(!strcmp(encoded, challenge));
  CHECK(mulib_soauth_pkce_challenge(p, text("short"), encoded) == MULIB_SC_INVALID);
  CHECK(mulib_soauth_pkce(p, encoded, encoded + 1) == MULIB_SC_INVALID);

  CHECK(!mulib_sjose_sign(p, k, text("abc"), encoded, sizeof encoded, &n));
  mulib_sc_buf out = {decoded, sizeof decoded};
  size_t written = 0;
  CHECK(!mulib_sjose_verify(p, k, encoded, n, out, &written));
  CHECK(written == 3 && !memcmp(decoded, "abc", 3));
  memset(decoded, 0x55, sizeof decoded);
  encoded[n - 43] = encoded[n - 43] == 'A' ? 'B' : 'A';
  CHECK(mulib_sjose_verify(p, k, encoded, n, out, &written) == MULIB_SC_AUTH);
  CHECK(written == 3 && decoded[0] == 0x55);

  unsigned char packet[] = {13, 0,0,0,1, 9, 0,0,0,1, 8, 0,0,0,2};
  mulib_sc_view in = {packet, sizeof packet};
  mulib_ssh_sign_request parsed;
  CHECK(!mulib_ssh_secret_parse_sign(in, &parsed) && parsed.flags == 2);
  packet[sizeof packet - 1] = 1;
  CHECK(mulib_ssh_secret_parse_sign(in, &parsed) == MULIB_SC_UNSUPPORTED);
  --in.len;
  CHECK(mulib_ssh_secret_parse_sign(in, &parsed) == MULIB_SC_INVALID);
  CHECK(parsed.flags == 2);

  CHECK(!mulib_saws_key(p, text("wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY"),
                       "20120215", "us-east-1", "iam", digest));
  CHECK(!mulib_scode_encode(MULIB_SCODE_HEX, mac, encoded, sizeof encoded, &n));
  CHECK(!strcmp(encoded, "f4780e2d9f65fa895f9c67b32ce1baf0b0d8a43505a000a1a9e090d414db404d"));
  CHECK(mulib_saws_key(p, k, "20120", "us-east-1", "iam", digest) == MULIB_SC_INVALID);

  mulib_ssign_key signing = {{0}, MULIB_SSIGN_ED25519, MULIB_SSIGN_SIGN};
  CHECK(mulib_ssign_sign(NULL, &signing, text("abc"), out, &written) == MULIB_SC_LIMIT);
  mulib_sc_buf sigbuf = {(unsigned char *)encoded, sizeof encoded};
  CHECK(mulib_ssign_sign(NULL, &signing, text("abc"), sigbuf, &written)
        == MULIB_SC_UNSUPPORTED);

  unsigned char authdata[37];
  CHECK(!mulib_swa_authdata(p, "example.test", 0x01020304, 1, 1, authdata));
  CHECK(!memcmp(authdata + 32, "\x05\x01\x02\x03\x04", 5));
  CHECK(mulib_swa_authdata(p, "example..test", 0, 1, 1, authdata) == MULIB_SC_INVALID);
  CHECK(mulib_swa_authdata(p, "-example.test", 0, 1, 1, authdata) == MULIB_SC_INVALID);
  CHECK(mulib_swa_authdata(p, "example.test", 0, 1, 2, authdata) == MULIB_SC_INVALID);
  CHECK(!memcmp(authdata + 32, "\x05\x01\x02\x03\x04", 5));

  unsigned char shares[4 * 4];
  mulib_sc_buf split = {shares, sizeof shares};
  CHECK(!mulib_srec_split(p, text("abc"), 2, 4, split));
  mulib_sc_view pair = {shares + 4, 8};
  CHECK(!mulib_srec_join(pair, 3, 2, 2, out) && !memcmp(decoded, "abc", 3));
  shares[8] = shares[4];
  CHECK(mulib_srec_join(pair, 3, 2, 2, out) == MULIB_SC_INVALID);
}

int
main(void)
{
  codecs();
  uris();
  mulib_scr_provider provider;
  mulib_scr_native(&provider);
  otp(&provider);
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
  const int have_native_entropy = 1;
#else
  const int have_native_entropy = 0;
#endif
  if (have_native_entropy) {
    generators(&provider);
    protocol_primitives(&provider);
  }
#ifdef MULIB_TEST_SECRET_OPENSSL
  mulib_scr_openssl(&provider);
  otp(&provider);
  generators(&provider);
  protocol_primitives(&provider);
#endif
  return EXIT_SUCCESS;
}
