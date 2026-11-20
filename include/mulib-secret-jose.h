/** @file mulib-secret-jose.h
 * @brief HS256 compact JWS signing and signature verification with fixed protected header.
 * Define MULIB_IMPL_SECRET_JOSE once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_JOSE_H
#define MULIB_SECRET_JOSE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-codec.h"
#include "mulib-secret-crypto.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Payload is up to 65536 bytes; this does not validate JWT claims or JSON.
 * Require exact protected header {"alg":"HS256","typ":"JWT"}.
 * Input/output/written may not overlap. Sign adds a NUL, not counted.
 * Invalid arguments leave output unchanged. Sign clears bytes it may have
 * written on operational failure; verify never releases data on failure. */
int mulib_sjose_sign(const mulib_scr_provider *p,mulib_sc_view key,mulib_sc_view payload,char *out,size_t cap,size_t *written);
int mulib_sjose_verify(const mulib_scr_provider *p,mulib_sc_view key,const char *token,size_t n,mulib_sc_buf payload,size_t *written);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_JOSE) && !defined(MULIB_SECRET_JOSE_IMPLEMENTED)
#define MULIB_SECRET_JOSE_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static const char mulib_sjose_header_[] = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.";
#define MULIB_SJOSE_MAX_PAYLOAD_ 65536u

int
mulib_sjose_sign(const mulib_scr_provider *p, mulib_sc_view key, mulib_sc_view payload,
                 char *out, size_t cap, size_t *written)
{
  if (!out || !written || !key.ptr || key.len < 32 || !mulib_sc_view_ok(payload)
      || payload.len > MULIB_SJOSE_MAX_PAYLOAD_) return MULIB_SC_INVALID;
  size_t header = sizeof mulib_sjose_header_ - 1;
  size_t need = header + (payload.len * 8 + 5) / 6 + 45;
  if (cap < need) return MULIB_SC_LIMIT;
  if (mulib_sc_overlap(out, need, payload.ptr, payload.len)
      || mulib_sc_overlap(out, need, key.ptr, key.len)
      || mulib_sc_overlap(out, need, written, sizeof *written)
      || mulib_sc_overlap(written, sizeof *written, key.ptr, key.len)
      || mulib_sc_overlap(written, sizeof *written, payload.ptr, payload.len))
    return MULIB_SC_INVALID;
  memcpy(out, mulib_sjose_header_, header);
  size_t body = 0, tail = 0;
  int rc = mulib_scode_encode(MULIB_SCODE_BASE64URL, payload, out + header, cap - header,
                              &body);
  unsigned char mac[32];
  if (!rc) {
    mulib_sc_view msg = {(const unsigned char *)out, header + body};
    rc = mulib_scr_hmac(p, MULIB_SCR_SHA256, key, msg, mac, sizeof mac);
  }
  if (!rc) {
    out[header + body] = '.';
    mulib_sc_view sig = {mac, sizeof mac};
    rc = mulib_scode_encode(MULIB_SCODE_BASE64URL, sig, out + header + body + 1,
                             cap - header - body - 1, &tail);
  }
  mulib_sc_wipe(mac, sizeof mac);
  if (rc) mulib_sc_wipe(out, need);
  else *written = header + body + 1 + tail;
  return rc;
}

int
mulib_sjose_verify(const mulib_scr_provider *p, mulib_sc_view key, const char *token,
                   size_t n, mulib_sc_buf out, size_t *written)
{
  size_t header = sizeof mulib_sjose_header_ - 1;
  if (!token || !written || !key.ptr || key.len < 32 || (!out.ptr && out.cap)
      || n < header + 44 || n > header + 44 + (MULIB_SJOSE_MAX_PAYLOAD_ * 8 + 5) / 6
      || memcmp(token, mulib_sjose_header_, header)) return MULIB_SC_INVALID;
  size_t dot = n - 44;
  if (token[dot] != '.' || mulib_sc_overlap(out.ptr, out.cap, token, n)
      || mulib_sc_overlap(out.ptr, out.cap, key.ptr, key.len)
      || mulib_sc_overlap(written, sizeof *written, token, n)
      || mulib_sc_overlap(written, sizeof *written, key.ptr, key.len)
      || mulib_sc_overlap(written, sizeof *written, out.ptr, out.cap))
    return MULIB_SC_INVALID;
  unsigned char sig[32], mac[32];
  size_t size = 0;
  mulib_sc_buf sigbuf = {sig, sizeof sig};
  int rc = mulib_scode_decode(MULIB_SCODE_BASE64URL, token + dot + 1, 43, sigbuf, &size);
  if (!rc && size != sizeof sig) rc = MULIB_SC_INVALID;
  if (!rc) {
    mulib_sc_view msg = {(const unsigned char *)token, dot};
    rc = mulib_scr_hmac(p, MULIB_SCR_SHA256, key, msg, mac, sizeof mac);
  }
  if (!rc && !mulib_sc_equal(mac, sig, sizeof mac)) rc = MULIB_SC_AUTH;
  mulib_sc_wipe(mac, sizeof mac);
  mulib_sc_wipe(sig, sizeof sig);
  if (rc) return rc;
  return mulib_scode_decode(MULIB_SCODE_BASE64URL, token + header, dot - header,
                            out, written);
}
#undef MULIB_SJOSE_MAX_PAYLOAD_
#ifdef __cplusplus
}
#endif
#endif
