/** @file mulib-git.h
 * @brief Git SHA-1 object identities and pkt-line framing.
 * Define MULIB_IMPL_GIT once. SHA-1 is for legacy Git compatibility, not
 * authentication or collision-resistant security. No command execution.
 */
#ifndef MULIB_GIT_H
#define MULIB_GIT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_git_oid
  {
    unsigned char bytes[20];
  } mulib_git_oid_t;
  /** @brief Hash arbitrary bytes; 1 success, 0 invalid/length overflow. */
  int mulib_git_sha1 (const void *, size_t, mulib_git_oid_t *);
  /** @brief Hash canonical "type length\0content" for blob/tree/commit/tag. */
  int mulib_git_object_id (const char *type, const void *, size_t,
                           mulib_git_oid_t *);
  /** @brief Format 40 lowercase hex digits plus NUL; parse exactly 40 hex
   * bytes. */
  int mulib_git_oid_format (const mulib_git_oid_t *, char out[41]);
  int mulib_git_oid_parse (const char *, size_t, mulib_git_oid_t *);
  /** @brief Decode pkt-line, 1 success, 0 incomplete, -1 invalid. kind=0 data,
   * 1 flush (0000), 2 delimiter (0001), 3 response-end (0002). Data is
   * borrowed; used is zero on failure. Payload maximum 65516. */
  int mulib_git_pkt_decode (const void *, size_t,
                            const unsigned char **payload, size_t *length,
                            unsigned *kind, size_t *used);
  /** @brief Encode data or control pkt-line, 1 success/0 failure. */
  int mulib_git_pkt_encode (unsigned kind, const void *, size_t, void *,
                            size_t, size_t *used);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_GIT) && !defined(MULIB_GIT_IMPLEMENTED)
#define MULIB_GIT_IMPLEMENTED
#include <stdio.h>
#include <string.h>
typedef struct mulib_git_sha1_state_
{
  uint32_t h[5];
  uint64_t bytes;
  unsigned char block[64];
  size_t used;
} mulib_git_sha1_state_;
static uint32_t
mulib_git_rol_ (uint32_t v, unsigned n)
{
  return (v << n) | (v >> (32 - n));
}
static void
mulib_git_sha1_block_ (mulib_git_sha1_state_ *s)
{
  uint32_t w[80], a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3],
                  e = s->h[4];
  for (unsigned i = 0; i < 16; ++i)
    {
      const unsigned char *p = s->block + 4 * i;
      w[i] = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8
             | p[3];
    }
  for (unsigned i = 16; i < 80; ++i)
    w[i] = mulib_git_rol_ (w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
  for (unsigned i = 0; i < 80; ++i)
    {
      uint32_t f, k, t;
      if (i < 20)
        {
          f = (b & c) | (~b & d);
          k = UINT32_C (0x5a827999);
        }
      else if (i < 40)
        {
          f = b ^ c ^ d;
          k = UINT32_C (0x6ed9eba1);
        }
      else if (i < 60)
        {
          f = (b & c) | (b & d) | (c & d);
          k = UINT32_C (0x8f1bbcdc);
        }
      else
        {
          f = b ^ c ^ d;
          k = UINT32_C (0xca62c1d6);
        }
      t = mulib_git_rol_ (a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = mulib_git_rol_ (b, 30);
      b = a;
      a = t;
    }
  s->h[0] += a;
  s->h[1] += b;
  s->h[2] += c;
  s->h[3] += d;
  s->h[4] += e;
}
static void
mulib_git_sha1_init_ (mulib_git_sha1_state_ *s)
{
  memset (s, 0, sizeof (*s));
  s->h[0] = UINT32_C (0x67452301);
  s->h[1] = UINT32_C (0xefcdab89);
  s->h[2] = UINT32_C (0x98badcfe);
  s->h[3] = UINT32_C (0x10325476);
  s->h[4] = UINT32_C (0xc3d2e1f0);
}
static void
mulib_git_sha1_update_ (mulib_git_sha1_state_ *s, const void *data, size_t n)
{
  const unsigned char *p = data;
  s->bytes += n;
  while (n)
    {
      size_t take = 64 - s->used;
      if (take > n)
        take = n;
      memcpy (s->block + s->used, p, take);
      p += take;
      n -= take;
      s->used += take;
      if (s->used == 64)
        {
          mulib_git_sha1_block_ (s);
          s->used = 0;
        }
    }
}
static void
mulib_git_sha1_final_ (mulib_git_sha1_state_ *s, mulib_git_oid_t *out)
{
  uint64_t bits = s->bytes * 8;
  s->block[s->used++] = 128;
  if (s->used > 56)
    {
      memset (s->block + s->used, 0, 64 - s->used);
      mulib_git_sha1_block_ (s);
      s->used = 0;
    }
  memset (s->block + s->used, 0, 56 - s->used);
  for (unsigned i = 0; i < 8; ++i)
    s->block[63 - i] = (unsigned char)(bits >> (8 * i));
  mulib_git_sha1_block_ (s);
  for (unsigned i = 0; i < 20; ++i)
    out->bytes[i] = (unsigned char)(s->h[i / 4] >> (24 - 8 * (i % 4)));
}
int
mulib_git_sha1 (const void *p, size_t n, mulib_git_oid_t *out)
{
  mulib_git_sha1_state_ s;
  if ((!p && n) || !out || n > UINT64_MAX / 8)
    return 0;
  mulib_git_sha1_init_ (&s);
  mulib_git_sha1_update_ (&s, p, n);
  mulib_git_sha1_final_ (&s, out);
  return 1;
}
int
mulib_git_object_id (const char *type, const void *p, size_t n,
                     mulib_git_oid_t *out)
{
  mulib_git_sha1_state_ s;
  char header[64];
  int k;
  if (!type || (!p && n) || !out
      || (strcmp (type, "blob") && strcmp (type, "tree")
          && strcmp (type, "commit") && strcmp (type, "tag")))
    return 0;
  k = snprintf (header, sizeof (header), "%s %zu", type, n);
  if (k < 0 || (size_t)k >= sizeof (header)
      || n > UINT64_MAX / 8 - (size_t)k - 1)
    return 0;
  mulib_git_sha1_init_ (&s);
  mulib_git_sha1_update_ (&s, header, (size_t)k + 1);
  mulib_git_sha1_update_ (&s, p, n);
  mulib_git_sha1_final_ (&s, out);
  return 1;
}
static int
mulib_git_hex_ (unsigned char c)
{
  return c >= '0' && c <= '9'   ? c - '0'
         : c >= 'a' && c <= 'f' ? c - 'a' + 10
         : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                : -1;
}
int
mulib_git_oid_format (const mulib_git_oid_t *id, char out[41])
{
  const char *hex = "0123456789abcdef";
  if (!id || !out)
    return 0;
  for (unsigned i = 0; i < 20; ++i)
    {
      out[2 * i] = hex[id->bytes[i] >> 4];
      out[2 * i + 1] = hex[id->bytes[i] & 15];
    }
  out[40] = 0;
  return 1;
}
int
mulib_git_oid_parse (const char *p, size_t n, mulib_git_oid_t *out)
{
  mulib_git_oid_t id;
  if (!p || !out || n != 40)
    return 0;
  for (unsigned i = 0; i < 20; ++i)
    {
      int a = mulib_git_hex_ ((unsigned char)p[2 * i]),
          b = mulib_git_hex_ ((unsigned char)p[2 * i + 1]);
      if (a < 0 || b < 0)
        return 0;
      id.bytes[i] = (unsigned char)(a * 16 + b);
    }
  *out = id;
  return 1;
}
int
mulib_git_pkt_decode (const void *data, size_t n,
                      const unsigned char **payload, size_t *length,
                      unsigned *kind, size_t *used)
{
  const unsigned char *p = data;
  unsigned v = 0;
  if (used)
    *used = 0;
  if ((!p && n) || !payload || !length || !kind || !used)
    return -1;
  for (size_t i = 0; i < n && i < 4; ++i)
    {
      int d = mulib_git_hex_ (p[i]);
      if (d < 0)
        return -1;
      v = v * 16 + (unsigned)d;
    }
  if (n < 4)
    return 0;
  if (v == 3 || v > 65520)
    return -1;
  if (v && v < 3)
    {
      *kind = v + 1;
      *length = 0;
      *payload = NULL;
      *used = 4;
      return 1;
    }
  if (!v)
    {
      *kind = 1;
      *length = 0;
      *payload = NULL;
      *used = 4;
      return 1;
    }
  if (n < v)
    return 0;
  *kind = 0;
  *length = v - 4;
  *payload = p + 4;
  *used = v;
  return 1;
}
int
mulib_git_pkt_encode (unsigned kind, const void *data, size_t n, void *out,
                      size_t cap, size_t *used)
{
  unsigned char *p = out;
  unsigned v;
  const char *hex = "0123456789abcdef";
  if (used)
    *used = 0;
  if (!p || !used || kind > 3 || (!data && n) || (kind && n) || n > 65516
      || cap < 4 || n > cap - 4)
    return 0;
  v = kind ? kind - 1 : (unsigned)n + 4;
  for (unsigned i = 0; i < 4; ++i)
    p[i] = (unsigned char)hex[(v >> (12 - i * 4)) & 15];
  if (n)
    memcpy (p + 4, data, n);
  *used = n + 4;
  return 1;
}
#endif
