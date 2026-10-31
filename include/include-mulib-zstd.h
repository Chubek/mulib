/** @file include-mulib-zstd.h
 * @brief Dependency-free Zstandard single-segment raw/RLE frame codec.
 * Static-inline implementation. Standard magic, content sizes and 128 KiB block
 * bounds are supported. Entropy-compressed blocks, dictionaries, checksums and
 * multi-frame/skippable streams return ENOTSUP or EFORMAT; see the manual. */
#ifndef INCLUDE_MULIB_ZSTD_H
#define INCLUDE_MULIB_ZSTD_H
#include "include-mulib-container-core.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Encode a standard frame; runs of at least eight bytes use RLE blocks.
 * Output is an initialized blob replaced atomically, and may alias input. */
static inline int
mulib_zstd_raw_encode (const void *data, size_t n, mulib_ct_blob *out, size_t limit)
{
  const unsigned char *p = (const unsigned char *)data;
  mulib_ct_blob next = { NULL, 0, 0 };
  unsigned char header[13] = { 0x28, 0xb5, 0x2f, 0xfd, 0xe0, 0, 0, 0, 0, 0, 0, 0, 0 };
  size_t at = 0;
  int rc;
  if ((!p && n) || !out) return MULIB_CT_EINVAL;
  mulib_ct_store64 (header + 5, n);
  rc = mulib_ct_blob_append (&next, header, sizeof (header), limit);
  while (!rc)
    {
      size_t run = 0, length, start = at;
      unsigned type = 0;
      uint32_t block;
      unsigned char h[3];
      while (run < n - at && run < 131072 && p[at + run] == p[at]) ++run;
      if (run >= 8) { type = 1; length = run; }
      else
        {
          length = 0;
          while (length < n - at && length < 131072)
            {
              size_t k = 1;
              while (k < 8 && k < n - at - length && p[at + length + k] == p[at + length]) ++k;
              if (k >= 8) break;
              ++length;
            }
        }
      at += length;
      block = (uint32_t)(length << 3) | (type << 1) | (at == n);
      h[0] = (unsigned char)block; h[1] = (unsigned char)(block >> 8); h[2] = (unsigned char)(block >> 16);
      rc = mulib_ct_blob_append (&next, h, 3, limit);
      if (!rc && length) rc = mulib_ct_blob_append (&next, p + start, type ? 1 : length, limit);
      if (at == n) break;
    }
  if (rc) { mulib_ct_blob_free (&next); return rc; }
  mulib_ct_blob_free (out); *out = next;
  return 0;
}
/* Walk block metadata before allocation, then repeat to copy the validated bytes. */
static inline int
mulib_zstd_raw_blocks_ (const unsigned char *p, size_t n, size_t at,
                        size_t size, unsigned char *out)
{
  size_t produced = 0;
  int last = 0;
  while (!last)
    {
      uint32_t block;
      size_t length, stored;
      unsigned type;
      if (n - at < 3) return MULIB_CT_EFORMAT;
      block = (uint32_t)p[at] | (uint32_t)p[at + 1] << 8 | (uint32_t)p[at + 2] << 16;
      at += 3; last = block & 1; type = (block >> 1) & 3; length = block >> 3;
      if (type == 2) return MULIB_CT_ENOTSUP;
      if (type == 3 || length > 131072 || length > size - produced) return MULIB_CT_EFORMAT;
      stored = type == 1 ? 1 : length;
      if (stored > n - at) return MULIB_CT_EFORMAT;
      if (out && length)
        {
          if (type) memset (out + produced, p[at], length);
          else memcpy (out + produced, p + at, length);
        }
      produced += length; at += stored;
    }
  return at == n && produced == size ? 0 : MULIB_CT_EFORMAT;
}
/** @brief Decode one complete supported frame. Bound advertised output and
 * validate every block before allocating. Input must stay immutable through
 * both passes. Reject truncation, excess bytes and content-size mismatch. */
static inline int
mulib_zstd_raw_decode (const void *data, size_t n, mulib_ct_blob *out, size_t limit)
{
  const unsigned char *p = (const unsigned char *)data;
  mulib_ct_blob next = { NULL, 0, 0 };
  uint64_t size = 0;
  size_t field, i;
  unsigned descriptor;
  int rc;
  if (!p || !out) return MULIB_CT_EINVAL;
  if (n < 5 || mulib_ct_load32 (p) != UINT32_C(0xfd2fb528)) return MULIB_CT_EFORMAT;
  descriptor = p[4];
  if (descriptor & 0x18U) return MULIB_CT_EFORMAT;
  if (!(descriptor & 0x20U) || (descriptor & 7U)) return MULIB_CT_ENOTSUP;
  field = (descriptor >> 6) == 0 ? 1 : (size_t)1 << (descriptor >> 6);
  if (n - 5 < field) return MULIB_CT_EFORMAT;
  for (i = 0; i < field; ++i) size |= (uint64_t)p[5 + i] << (8 * i);
  if (field == 2) size += 256;
  if (size > limit || size > SIZE_MAX) return MULIB_CT_ELIMIT;
  rc = mulib_zstd_raw_blocks_ (p, n, 5 + field, (size_t)size, NULL);
  if (rc) return rc;
  rc = mulib_ct_blob_reserve (&next, (size_t)size, limit);
  if (rc) return rc;
  rc = mulib_zstd_raw_blocks_ (p, n, 5 + field, (size_t)size, next.data);
  if (rc) { mulib_ct_blob_free (&next); return rc; }
  next.length = (size_t)size;
  mulib_ct_blob_free (out); *out = next;
  return 0;
}
#ifdef __cplusplus
}
#endif
#endif
