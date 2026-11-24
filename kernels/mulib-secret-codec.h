/** @file mulib-secret-codec.h
 * @brief Canonical unpadded Base64url/Base32 and lowercase hexadecimal encoding.
 * Define MULIB_IMPL_SECRET_CODEC once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_CODEC_H
#define MULIB_SECRET_CODEC_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-core.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { MULIB_SCODE_BASE64URL=1,MULIB_SCODE_BASE32=2,MULIB_SCODE_HEX=3 };
/* Unpadded RFC 4648; Base32 uppercase and hex lowercase only. Encode adds a
 * NUL not included in written. Decode consumes exactly n bytes and adds no NUL.
 * Input, output and written must be disjoint. On error output and written
 * are unchanged. Empty input is allowed. */
int mulib_scode_encode(unsigned format,mulib_sc_view in,char *out,size_t cap,size_t *written);
int mulib_scode_decode(unsigned format,const char *in,size_t n,mulib_sc_buf out,size_t *written);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_CODEC) && !defined(MULIB_SECRET_CODEC_IMPLEMENTED)
#define MULIB_SECRET_CODEC_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static const char *
mulib_scode_alphabet_(unsigned format, unsigned *bits)
{
  switch (format) {
    case MULIB_SCODE_BASE64URL:
      *bits = 6;
      return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    case MULIB_SCODE_BASE32:
      *bits = 5;
      return "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    case MULIB_SCODE_HEX:
      *bits = 4;
      return "0123456789abcdef";
    default:
      return NULL;
  }
}

int
mulib_scode_encode(unsigned format, mulib_sc_view in, char *out, size_t cap,
                    size_t *written)
{
  unsigned width;
  const char *alphabet = mulib_scode_alphabet_(format, &width);
  if (!alphabet || !out || !written || !mulib_sc_view_ok(in))
    return MULIB_SC_INVALID;
  size_t head = in.len / width;
  size_t tail = ((in.len % width) * 8 + width - 1) / width;
  size_t need;
  if (head > SIZE_MAX / 8 || mulib_sc_add(head * 8, tail, &need) || cap <= need)
    return MULIB_SC_LIMIT;
  if (mulib_sc_overlap(in.ptr, in.len, out, need + 1)
      || mulib_sc_overlap(written, sizeof *written, out, need + 1)
      || mulib_sc_overlap(written, sizeof *written, in.ptr, in.len))
    return MULIB_SC_INVALID;
  unsigned acc = 0, bits = 0;
  unsigned mask = (1u << width) - 1;
  size_t j = 0;
  for (size_t i = 0; i < in.len; ++i) {
    acc = (acc << 8) | in.ptr[i];
    bits += 8;
    while (bits >= width) {
      bits -= width;
      out[j++] = alphabet[(acc >> bits) & mask];
    }
  }
  if (bits) out[j++] = alphabet[(acc << (width - bits)) & mask];
  out[j] = 0;
  *written = j;
  return MULIB_SC_OK;
}

int
mulib_scode_decode(unsigned format, const char *in, size_t n, mulib_sc_buf out,
                    size_t *written)
{
  unsigned width;
  const char *alphabet = mulib_scode_alphabet_(format, &width);
  if (!alphabet || (!in && n) || !written || (!out.ptr && out.cap))
    return MULIB_SC_INVALID;
  unsigned tail = (unsigned)(n % 8) * width % 8;
  size_t need = n / 8 * width + n % 8 * width / 8;
  if (tail >= width) return MULIB_SC_INVALID;
  if (need > out.cap) return MULIB_SC_LIMIT;
  if (mulib_sc_overlap(in, n, out.ptr, need)
      || mulib_sc_overlap(written, sizeof *written, out.ptr, need)
      || mulib_sc_overlap(written, sizeof *written, in, n))
    return MULIB_SC_INVALID;

  /* Validate every byte before committing any output. The tail bits, if
   * present, belong entirely to the final symbol and must all be zero. */
  for (size_t i = 0; i < n; ++i) {
    const char *symbol = in[i] ? strchr(alphabet, in[i]) : NULL;
    if (!symbol || (i == n - 1 && ((unsigned)(symbol - alphabet) & ((1u << tail) - 1))))
      return MULIB_SC_INVALID;
  }
  unsigned acc = 0, bits = 0;
  size_t j = 0;
  for (size_t i = 0; i < n; ++i) {
    acc = (acc << width) | (unsigned)(strchr(alphabet, in[i]) - alphabet);
    bits += width;
    if (bits >= 8) {
      bits -= 8;
      out.ptr[j++] = (unsigned char)(acc >> bits);
    }
  }
  *written = need;
  return MULIB_SC_OK;
}
#ifdef __cplusplus
}
#endif
#endif
