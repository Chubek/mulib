/** @file mulib-secret-otp-uri.h
 * @brief Bounded otpauth URI import for Google Authenticator compatible secrets.
 * Define MULIB_IMPL_SECRET_OTP_URI once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_OTP_URI_H
#define MULIB_SECRET_OTP_URI_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-codec.h"
#include "mulib-secret-otp.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
  unsigned char key[128];
  size_t key_length;
  unsigned hash, digits, hotp;
  uint32_t period;
  uint64_t counter;
  char label[256], issuer[128];
} mulib_sotp_uri;
/* Strict single-segment otpauth URI, no fragment, duplicate or unknown
 * query keys. Percent-decoded text must be valid UTF-8 without C0/C1 controls;
 * '+' remains literal. Base32 accepts either case and optional exact RFC 4648
 * padding; noncanonical tail bits are rejected. If there is an issuer prefix,
 * any issuer query must match it byte for byte. Output is unchanged on error.
 * URI and output must not overlap. Wipe the output and never log the URI. */
int mulib_sotp_uri_parse(const char *uri,size_t length,mulib_sotp_uri *out);
void mulib_sotp_uri_clear(mulib_sotp_uri *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_OTP_URI) && !defined(MULIB_SECRET_OTP_URI_IMPLEMENTED)
#define MULIB_SECRET_OTP_URI_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static int
mulib_sotp_uri_hex_(unsigned c)
{
  if (c >= '0' && c <= '9') return (int)(c - '0');
  if (c >= 'a' && c <= 'f') return (int)(c - 'a' + 10);
  if (c >= 'A' && c <= 'F') return (int)(c - 'A' + 10);
  return -1;
}

static int
mulib_sotp_uri_utf8_(const char *s, size_t n)
{
  for (size_t i = 0; i < n;) {
    unsigned c = (unsigned char)s[i++];
    if (c < 128) {
      if (c < 32 || c == 127) return 0;
      continue;
    }
    uint32_t value, minimum;
    size_t more;
    if (c >= 0xc2 && c <= 0xdf) {
      value = c & 31; minimum = 0x80; more = 1;
    } else if (c >= 0xe0 && c <= 0xef) {
      value = c & 15; minimum = 0x800; more = 2;
    } else if (c >= 0xf0 && c <= 0xf4) {
      value = c & 7; minimum = 0x10000; more = 3;
    } else return 0;
    if (more > n - i) return 0;
    while (more--) {
      c = (unsigned char)s[i++];
      if ((c & 0xc0) != 0x80) return 0;
      value = (value << 6) | (c & 63);
    }
    if (value < minimum || value < 0xa0 || value > 0x10ffff
        || (value >= 0xd800 && value <= 0xdfff)) return 0;
  }
  return 1;
}

static int
mulib_sotp_uri_dec_(const char *s, size_t n, int query, char *out, size_t cap)
{
  size_t pos = 0;
  for (size_t i = 0; i < n; ++i) {
    unsigned c = (unsigned char)s[i];
    if (c == '%') {
      if (n - i < 3) return 0;
      int high = mulib_sotp_uri_hex_((unsigned char)s[i + 1]);
      int low = mulib_sotp_uri_hex_((unsigned char)s[i + 2]);
      if (high < 0 || low < 0) return 0;
      c = (unsigned)(16 * high + low);
      i += 2;
    } else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                 || (c >= '0' && c <= '9')
                 || (c && c < 128 && strchr("-._~!$&'()*+,;=:@", (int)c))
                 || (query && (c == '/' || c == '?')))) return 0;
    if (pos + 1 >= cap) return 0;
    out[pos++] = (char)c;
  }
  out[pos] = 0;
  return pos && mulib_sotp_uri_utf8_(out, pos);
}

static int
mulib_sotp_uri_num_(const char *s, uint64_t *value)
{
  uint64_t n = 0;
  if (!*s) return 0;
  for (; *s; ++s) {
    if (*s < '0' || *s > '9') return 0;
    unsigned digit = (unsigned)(*s - '0');
    if (n > (UINT64_MAX - digit) / 10) return 0;
    n = n * 10 + digit;
  }
  *value = n;
  return 1;
}

void
mulib_sotp_uri_clear(mulib_sotp_uri *out)
{
  if (out) mulib_sc_wipe(out, sizeof *out);
}

int
mulib_sotp_uri_parse(const char *uri, size_t len, mulib_sotp_uri *out)
{
  enum { SECRET=1, ISSUER=2, ALGORITHM=4, DIGITS=8, PERIOD=16, COUNTER=32 };
  if (!uri || !out || len < 16 || len > 4096
      || mulib_sc_overlap(uri, len, out, sizeof *out)
      || memchr(uri, 0, len) || memchr(uri, '#', len)) return MULIB_SC_INVALID;
  mulib_sotp_uri parsed;
  memset(&parsed, 0, sizeof parsed);
  parsed.hash = MULIB_SCR_SHA1;
  parsed.digits = 6;
  parsed.period = 30;
  if (!memcmp(uri, "otpauth://hotp/", 15)) parsed.hotp = 1;
  else if (memcmp(uri, "otpauth://totp/", 15)) return MULIB_SC_INVALID;
  const char *query = (const char *)memchr(uri + 15, '?', len - 15);
  size_t pos = 0;
  char value[512], name[32];
  unsigned seen = 0;
  int rc = MULIB_SC_INVALID;
  if (!query || !mulib_sotp_uri_dec_(uri + 15, (size_t)(query - uri) - 15, 0,
                                     parsed.label, sizeof parsed.label))
    goto done;

  pos = (size_t)(query - uri) + 1;
  while (pos < len) {
    size_t end = pos;
    while (end < len && uri[end] != '&') ++end;
    const char *eq = (const char *)memchr(uri + pos, '=', end - pos);
    if (!eq || !mulib_sotp_uri_dec_(uri + pos, (size_t)(eq - uri) - pos, 1,
                                    name, sizeof name)) goto done;
    unsigned field = !strcmp(name, "secret") ? SECRET : !strcmp(name, "issuer") ? ISSUER
                   : !strcmp(name, "algorithm") ? ALGORITHM : !strcmp(name, "digits") ? DIGITS
                   : !strcmp(name, "period") ? PERIOD : !strcmp(name, "counter") ? COUNTER : 0;
    if (!field || (seen & field)
        || !mulib_sotp_uri_dec_(eq + 1, end - (size_t)(eq - uri) - 1, 1,
                                value, sizeof value)) goto done;
    seen |= field;
    uint64_t n;
    switch (field) {
      case SECRET: {
        size_t total = strlen(value), raw = total;
        while (raw && value[raw - 1] == '=') --raw;
        if (raw != total && total - raw != (8 - raw % 8) % 8) goto done;
        for (size_t i = 0; i < raw; ++i)
          if (value[i] >= 'a' && value[i] <= 'z')
            value[i] = (char)(value[i] - 'a' + 'A');
        mulib_sc_buf key = {parsed.key, sizeof parsed.key};
        if (mulib_scode_decode(MULIB_SCODE_BASE32, value, raw, key, &parsed.key_length)
            || parsed.key_length < 10) goto done;
        break;
      }
      case ISSUER: {
        size_t size = strlen(value);
        if (size >= sizeof parsed.issuer || strchr(value, ':')) goto done;
        memcpy(parsed.issuer, value, size + 1);
        break;
      }
      case ALGORITHM:
        if (!strcmp(value, "SHA1")) parsed.hash = MULIB_SCR_SHA1;
        else if (!strcmp(value, "SHA256")) parsed.hash = MULIB_SCR_SHA256;
        else if (!strcmp(value, "SHA512")) parsed.hash = MULIB_SCR_SHA512;
        else goto done;
        break;
      case DIGITS:
        if (!mulib_sotp_uri_num_(value, &n) || (n != 6 && n != 8)) goto done;
        parsed.digits = (unsigned)n;
        break;
      case PERIOD:
        if (parsed.hotp || !mulib_sotp_uri_num_(value, &n) || !n || n > 86400)
          goto done;
        parsed.period = (uint32_t)n;
        break;
      case COUNTER:
        if (!parsed.hotp || !mulib_sotp_uri_num_(value, &parsed.counter)) goto done;
        break;
    }
    mulib_sc_wipe(value, sizeof value);
    if (end == len) break;
    pos = end + 1;
    if (pos == len) goto done;
  }
  if (!(seen & SECRET) || (parsed.hotp && !(seen & COUNTER))) goto done;
  {
    const char *colon = strchr(parsed.label, ':');
    const char *account = colon ? colon + 1 : parsed.label;
    while (*account == ' ') ++account;
    if (!*account || strchr(account, ':')) goto done;
    if (colon) {
      size_t prefix = (size_t)(colon - parsed.label);
      if (!prefix || prefix >= sizeof parsed.issuer) goto done;
      if (seen & ISSUER) {
        if (strlen(parsed.issuer) != prefix || memcmp(parsed.issuer, parsed.label, prefix))
          goto done;
      } else {
        memcpy(parsed.issuer, parsed.label, prefix);
        parsed.issuer[prefix] = 0;
      }
    }
  }
  *out = parsed;
  rc = MULIB_SC_OK;
done:
  mulib_sc_wipe(value, sizeof value);
  mulib_sc_wipe(name, sizeof name);
  mulib_sotp_uri_clear(&parsed);
  return rc;
}
#ifdef __cplusplus
}
#endif
#endif
