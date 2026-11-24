/** @file mulib-secret-ssh.h
 * @brief Bounded SSH agent sign-request parser and RFC 4251 string framing.
 * Define MULIB_IMPL_SECRET_SSH once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_SSH_H
#define MULIB_SECRET_SSH_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-core.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Payload excludes outer uint32 packet length. Borrowed views. Recognized
 * flags are 0, SHA2-256 (2), and SHA2-512 (4). Host validates the key blob,
 * algorithm and approved destination, and must not treat 0 as permission to
 * fall back to RSA/SHA1. Outputs are disjoint and unchanged on failure. */
typedef struct { mulib_sc_view public_blob,data; uint32_t flags; } mulib_ssh_sign_request;
int mulib_ssh_secret_parse_sign(mulib_sc_view packet,mulib_ssh_sign_request *out);
int mulib_ssh_secret_ed25519_blob(const unsigned char public_key[32],unsigned char out[51]);
int mulib_ssh_secret_signature_response(mulib_sc_view algorithm,mulib_sc_view signature,mulib_sc_buf out,size_t *written);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_SSH) && !defined(MULIB_SECRET_SSH_IMPLEMENTED)
#define MULIB_SECRET_SSH_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static uint32_t
mulib_ssh_secret_u32_(const unsigned char *p)
{
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static void
mulib_ssh_secret_put_(unsigned char *p, uint32_t v)
{
  for (unsigned i = 0; i < 4; ++i) p[3 - i] = (unsigned char)(v >> (i * 8));
}

static int
mulib_ssh_secret_string_(mulib_sc_view in, size_t *pos, mulib_sc_view *out)
{
  if (*pos > in.len || in.len - *pos < 4) return 0;
  uint32_t n = mulib_ssh_secret_u32_(in.ptr + *pos);
  *pos += 4;
  if (n > in.len - *pos) return 0;
  out->ptr = in.ptr + *pos;
  out->len = n;
  *pos += n;
  return 1;
}

int
mulib_ssh_secret_parse_sign(mulib_sc_view in, mulib_ssh_sign_request *out)
{
  if (!out || !in.ptr || in.len < 14 || in.len > 1048576 || in.ptr[0] != 13
      || mulib_sc_overlap(in.ptr, in.len, out, sizeof *out))
    return MULIB_SC_INVALID;
  size_t pos = 1;
  mulib_ssh_sign_request req;
  if (!mulib_ssh_secret_string_(in, &pos, &req.public_blob) || !req.public_blob.len
      || !mulib_ssh_secret_string_(in, &pos, &req.data) || in.len - pos != 4)
    return MULIB_SC_INVALID;
  req.flags = mulib_ssh_secret_u32_(in.ptr + pos);
  if (req.flags != 0 && req.flags != 2 && req.flags != 4) return MULIB_SC_UNSUPPORTED;
  *out = req;
  return MULIB_SC_OK;
}

int
mulib_ssh_secret_ed25519_blob(const unsigned char key[32], unsigned char out[51])
{
  if (!key || !out || mulib_sc_overlap(key, 32, out, 51)) return MULIB_SC_INVALID;
  mulib_ssh_secret_put_(out, 11);
  memcpy(out + 4, "ssh-ed25519", 11);
  mulib_ssh_secret_put_(out + 15, 32);
  memcpy(out + 19, key, 32);
  return MULIB_SC_OK;
}

int
mulib_ssh_secret_signature_response(mulib_sc_view alg, mulib_sc_view sig,
                                    mulib_sc_buf out, size_t *written)
{
  if (!alg.ptr || !sig.ptr || !written || !out.ptr || !alg.len || alg.len > 64
      || !sig.len || sig.len > 16384) return MULIB_SC_INVALID;
  size_t need = 13 + alg.len + sig.len;
  if (out.cap < need) return MULIB_SC_LIMIT;
  if (mulib_sc_overlap(alg.ptr, alg.len, out.ptr, need)
      || mulib_sc_overlap(sig.ptr, sig.len, out.ptr, need)
      || mulib_sc_overlap(written, sizeof *written, out.ptr, need)
      || mulib_sc_overlap(written, sizeof *written, alg.ptr, alg.len)
      || mulib_sc_overlap(written, sizeof *written, sig.ptr, sig.len))
    return MULIB_SC_INVALID;
  for (size_t i = 0; i < alg.len; ++i)
    if (alg.ptr[i] < 33 || alg.ptr[i] > 126 || alg.ptr[i] == ',') return MULIB_SC_INVALID;
  out.ptr[0] = 14;
  mulib_ssh_secret_put_(out.ptr + 1, (uint32_t)(8 + alg.len + sig.len));
  mulib_ssh_secret_put_(out.ptr + 5, (uint32_t)alg.len);
  memcpy(out.ptr + 9, alg.ptr, alg.len);
  mulib_ssh_secret_put_(out.ptr + 9 + alg.len, (uint32_t)sig.len);
  memcpy(out.ptr + 13 + alg.len, sig.ptr, sig.len);
  *written = need;
  return MULIB_SC_OK;
}
#ifdef __cplusplus
}
#endif
#endif
