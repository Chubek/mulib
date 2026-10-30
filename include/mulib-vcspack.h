/** @file mulib-vcspack.h
 * @brief Git PACK v2 encoding and bounded stored-DEFLATE pack decoding.
 * Define MULIB_IMPL_VCSPACK, GIT, ENDIAN and ADLER32 once. Emits real Git
 * packs; decoder supports non-delta objects with uncompressed zlib blocks.
 */
#ifndef MULIB_VCSPACK_H
#define MULIB_VCSPACK_H
#include "mulib-adler32.h"
#include "mulib-endian.h"
#include "mulib-git.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_vcspack_object
  {
    unsigned type; /* 1 commit, 2 tree, 3 blob, 4 tag */
    const void *data;
    size_t length;
  } mulib_vcspack_object_t;
  /** @brief Encode malloc-owned pack. 1 success/0 invalid, overflow or OOM.
   * Outputs unchanged on error. Inputs borrowed, <=UINT32_MAX objects. */
  int mulib_vcspack_encode (const mulib_vcspack_object_t *, size_t,
                            unsigned char **, size_t *);
  /** @brief Verify pack SHA-1 and zlib checksums then visit each object,
   * borrowing scratch bytes during callback. 1 complete, 0 malformed, -1
   * unsupported compression/delta, -2 object exceeds scratch, -3 callback
   * stopped. Earlier callbacks may have run before a later object fails. */
  int mulib_vcspack_decode (const void *, size_t, void *scratch, size_t,
                            int (*visit) (void *,
                                          const mulib_vcspack_object_t *),
                            void *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VCSPACK) && !defined(MULIB_VCSPACK_IMPLEMENTED)
#define MULIB_VCSPACK_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
int
mulib_vcspack_encode (const mulib_vcspack_object_t *objects, size_t count,
                      unsigned char **out, size_t *length)
{
  size_t cap = 32, pos = 12;
  unsigned char *p;
  mulib_git_oid_t id;
  if ((!objects && count) || !out || !length || count > UINT32_MAX)
    return 0;
  for (size_t i = 0; i < count; ++i)
    {
      size_t n = objects[i].length, blocks = n ? (n - 1) / 65535 + 1 : 1;
      if (objects[i].type < 1 || objects[i].type > 4 || (!objects[i].data && n)
          || blocks > (SIZE_MAX - 32) / 5 || n > SIZE_MAX - 32 - blocks * 5
          || cap > SIZE_MAX - n - 32 - blocks * 5)
        return 0;
      cap += n + 32 + blocks * 5;
    }
  p = malloc (cap);
  if (!p)
    return 0;
  memcpy (p, "PACK", 4);
  mulib_endian_store_be32 (p + 4, 2);
  mulib_endian_store_be32 (p + 8, (uint32_t)count);
  for (size_t i = 0; i < count; ++i)
    {
      size_t n = objects[i].length, v = n >> 4, off = 0;
      p[pos++]
          = (unsigned char)((objects[i].type << 4) | (n & 15) | (v ? 128 : 0));
      while (v)
        {
          unsigned char b = (unsigned char)(v & 127);
          v >>= 7;
          p[pos++] = (unsigned char)(b | (v ? 128 : 0));
        }
      p[pos++] = 0x78;
      p[pos++] = 0x01;
      do
        {
          size_t take = n - off;
          unsigned inv;
          if (take > 65535)
            take = 65535;
          p[pos++] = off + take == n ? 1 : 0;
          inv = (unsigned)take ^ 65535u;
          p[pos++] = (unsigned char)take;
          p[pos++] = (unsigned char)(take >> 8);
          p[pos++] = (unsigned char)inv;
          p[pos++] = (unsigned char)(inv >> 8);
          if (take)
            memcpy (p + pos, (const unsigned char *)objects[i].data + off,
                    take);
          pos += take;
          off += take;
        }
      while (off < n);
      mulib_endian_store_be32 (p + pos, mulib_adler32 (objects[i].data, n));
      pos += 4;
    }
  if (!mulib_git_sha1 (p, pos, &id))
    {
      free (p);
      return 0;
    }
  memcpy (p + pos, id.bytes, 20);
  *out = p;
  *length = pos + 20;
  return 1;
}
int
mulib_vcspack_decode (const void *data, size_t n, void *scratch, size_t cap,
                      int (*visit) (void *, const mulib_vcspack_object_t *),
                      void *ctx)
{
  const unsigned char *p = data;
  unsigned char *s = scratch;
  size_t pos = 12, end;
  uint32_t count;
  mulib_git_oid_t id;
  if (!p || n < 32 || (!s && cap) || !visit || memcmp (p, "PACK", 4)
      || mulib_endian_load_be32 (p + 4) != 2)
    return 0;
  end = n - 20;
  if (!mulib_git_sha1 (p, end, &id) || memcmp (id.bytes, p + end, 20))
    return 0;
  count = mulib_endian_load_be32 (p + 8);
  for (uint32_t i = 0; i < count; ++i)
    {
      mulib_vcspack_object_t o;
      unsigned b, shift = 4;
      size_t size, got = 0;
      int final = 0;
      if (pos == end)
        return 0;
      b = p[pos++];
      o.type = (b >> 4) & 7;
      size = b & 15;
      if (o.type < 1 || o.type > 4)
        return -1;
      while (b & 128)
        {
          if (pos == end || shift >= sizeof (size_t) * 8)
            return 0;
          b = p[pos++];
          if ((size_t)(b & 127) > (SIZE_MAX >> shift))
            return 0;
          size |= (size_t)(b & 127) << shift;
          shift += 7;
        }
      if (size > cap)
        return -2;
      if (end - pos < 2 || (p[pos] & 15) != 8 || (p[pos] >> 4) > 7
          || (((unsigned)p[pos] << 8) + p[pos + 1]) % 31)
        return 0;
      if (p[pos + 1] & 32)
        return -1;
      pos += 2;
      while (!final)
        {
          unsigned len, inv;
          if (pos == end)
            return 0;
          b = p[pos++];
          final = b & 1;
          if ((b >> 1) & 3)
            return -1;
          if (end - pos < 4)
            return 0;
          len = p[pos] | (unsigned)p[pos + 1] << 8;
          inv = p[pos + 2] | (unsigned)p[pos + 3] << 8;
          pos += 4;
          if ((len ^ inv) != 65535 || len > end - pos || len > size - got)
            return 0;
          if (len)
            memcpy (s + got, p + pos, len);
          got += len;
          pos += len;
        }
      if (got != size || end - pos < 4
          || mulib_adler32 (s, got) != mulib_endian_load_be32 (p + pos))
        return 0;
      pos += 4;
      o.data = s;
      o.length = got;
      if (visit (ctx, &o))
        return -3;
    }
  return pos == end;
}
#endif
