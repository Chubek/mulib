/** @file mulib-secret-aws.h
 * @brief AWS SigV4 signing-key derivation and canonical request signature primitive.
 * Define MULIB_IMPL_SECRET_AWS once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_AWS_H
#define MULIB_SECRET_AWS_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-crypto.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Date is eight YYYYMMDD digits; region/service are lowercase identifiers.
 * Callers must validate the timestamp, canonical request and endpoint policy.
 * Inputs and output may not overlap. Invalid arguments leave output unchanged;
 * operational errors clear it. */
int mulib_saws_key(const mulib_scr_provider *p,mulib_sc_view secret,const char *date,const char *region,const char *service,unsigned char out[32]);
int mulib_saws_sign(const mulib_scr_provider *p,const unsigned char signing_key[32],mulib_sc_view string_to_sign,char out[65]);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_AWS) && !defined(MULIB_SECRET_AWS_IMPLEMENTED)
#define MULIB_SECRET_AWS_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static size_t
mulib_saws_identifier_(const char *name)
{
  if (!name) return 0;
  size_t n = 0;
  for (; name[n]; ++n) {
    unsigned c = (unsigned char)name[n];
    if (n == 63 || !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
      return 0;
  }
  return n;
}

int
mulib_saws_key(const mulib_scr_provider *p, mulib_sc_view secret, const char *date,
                const char *region, const char *service, unsigned char out[32])
{
  if (!secret.ptr || !secret.len || secret.len > 256 || !date || !out)
    return MULIB_SC_INVALID;
  for (unsigned i = 0; i < 8; ++i)
    if (date[i] < '0' || date[i] > '9') return MULIB_SC_INVALID;
  size_t nr = mulib_saws_identifier_(region), ns = mulib_saws_identifier_(service);
  if (date[8] || !nr || !ns || mulib_sc_overlap(out, 32, secret.ptr, secret.len)
      || mulib_sc_overlap(out, 32, date, 9) || mulib_sc_overlap(out, 32, region, nr + 1)
      || mulib_sc_overlap(out, 32, service, ns + 1)) return MULIB_SC_INVALID;

  unsigned char base[260], key[32], next[32];
  memcpy(base, "AWS4", 4);
  memcpy(base + 4, secret.ptr, secret.len);
  mulib_sc_view start = {base, secret.len + 4};
  mulib_sc_view parts[] = {{(const unsigned char *)date, 8},
                           {(const unsigned char *)region, nr},
                           {(const unsigned char *)service, ns},
                           {(const unsigned char *)"aws4_request", 12}};
  int rc = mulib_scr_hmac(p, MULIB_SCR_SHA256, start, parts[0], key, sizeof key);
  mulib_sc_wipe(base, sizeof base);
  for (unsigned i = 1; !rc && i < sizeof parts / sizeof parts[0]; ++i) {
    mulib_sc_view current = {key, sizeof key};
    rc = mulib_scr_hmac(p, MULIB_SCR_SHA256, current, parts[i], next, sizeof next);
    if (!rc) memcpy(key, next, sizeof key);
  }
  if (rc) mulib_sc_wipe(out, 32);
  else memcpy(out, key, sizeof key);
  mulib_sc_wipe(key, sizeof key);
  mulib_sc_wipe(next, sizeof next);
  return rc;
}

int
mulib_saws_sign(const mulib_scr_provider *p, const unsigned char key[32],
                 mulib_sc_view message, char out[65])
{
  if (!key || !out || !mulib_sc_view_ok(message) || mulib_sc_overlap(out, 65, key, 32)
      || mulib_sc_overlap(out, 65, message.ptr, message.len)) return MULIB_SC_INVALID;
  unsigned char digest[32];
  mulib_sc_view k = {key, 32};
  int rc = mulib_scr_hmac(p, MULIB_SCR_SHA256, k, message, digest, sizeof digest);
  if (rc) mulib_sc_wipe(out, 65);
  else {
    static const char hex[] = "0123456789abcdef";
    for (unsigned i = 0; i < sizeof digest; ++i) {
      out[2 * i] = hex[digest[i] >> 4];
      out[2 * i + 1] = hex[digest[i] & 15];
    }
    out[64] = 0;
  }
  mulib_sc_wipe(digest, sizeof digest);
  return rc;
}
#ifdef __cplusplus
}
#endif
#endif
