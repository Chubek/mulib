/** @file mulib-container-packcoff.h
 * @brief COFF image objects and checksummed non-destructive file overlays.
 * Define MULIB_IMPL_CONTAINER_PACKCOFF plus IMGFORMAT/COFF dependencies. */
#ifndef MULIB_CONTAINER_PACKCOFF_H
#define MULIB_CONTAINER_PACKCOFF_H
#include "mulib-imgformat.h"
#include "mulib-coff.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Replace initialized blob with one .mulib data section; native COFF machine. */
int mulib_container_packcoff_object (const void *image, size_t length, uint16_t machine, mulib_ct_blob *out, size_t limit);
/** @brief Preserve original bytes and append image plus CRC locator atomically. */
int mulib_container_packcoff_append (const void *file, size_t file_length, const void *image, size_t image_length, mulib_ct_blob *out, size_t limit);
/** @brief Borrow validated image from final trailer or unique .mulib section.
 * All outputs unchanged on error; CRC is not authentication. */
int mulib_container_packcoff_open (const void *file, size_t length, size_t max_image, const unsigned char **image, size_t *image_length, mulib_imgformat_view *view);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINER_PACKCOFF) && !defined(MULIB_CONTAINER_PACKCOFF_IMPLEMENTED)
#define MULIB_CONTAINER_PACKCOFF_IMPLEMENTED
int mulib_container_packcoff_object (const void *image, size_t n, uint16_t machine, mulib_ct_blob *out, size_t limit)
{
  mulib_ct_blob next = { 0 };
  mulib_imgformat_view view;
  unsigned char *p;
  int rc;
  if (!out || !machine) return MULIB_CT_EINVAL;
  rc = mulib_imgformat_open (image, n, limit, &view); if (rc) return rc;
  if (n > UINT32_MAX - 60 || limit < 60 || n > limit - 60) return MULIB_CT_ELIMIT;
  rc = mulib_ct_blob_reserve (&next, n + 60, limit); if (rc) return rc;
  p = next.data; memset (p, 0, 60); next.length = n + 60;
  p[0] = (unsigned char)machine; p[1] = (unsigned char)(machine >> 8); p[2] = 1;
  memcpy (p + 20, ".mulib", 6); mulib_ct_store32 (p + 36, (uint32_t)n);
  mulib_ct_store32 (p + 40, 60); mulib_ct_store32 (p + 56, 0x40300040);
  memcpy (p + 60, image, n); mulib_ct_blob_free (out); *out = next; return 0;
}
int mulib_container_packcoff_append (const void *file, size_t file_n, const void *image, size_t image_n, mulib_ct_blob *out, size_t limit)
{
  mulib_coff_t v;
  mulib_imgformat_view view;
  mulib_ct_blob next = { 0 };
  unsigned char t[32] = { 0 };
  size_t total;
  int rc;
  if (!out) return MULIB_CT_EINVAL;
  if (mulib_coff_open (&v, file, file_n)) return MULIB_CT_EFORMAT;
  rc = mulib_imgformat_open (image, image_n, limit, &view); if (rc) return rc;
  if (mulib_ct_add_size (file_n, image_n, &total) || mulib_ct_add_size (total, 32, &total)) return MULIB_CT_ELIMIT;
  rc = mulib_ct_blob_reserve (&next, total, limit); if (rc) return rc;
  memcpy (t, "MCOFPK1", 7); mulib_ct_store64 (t + 8, file_n); mulib_ct_store64 (t + 16, image_n);
  mulib_ct_store32 (t + 24, mulib_crc32 (image, image_n)); mulib_ct_store32 (t + 28, mulib_crc32 (t, 28));
  memcpy (next.data, file, file_n); memcpy (next.data + file_n, image, image_n);
  memcpy (next.data + file_n + image_n, t, 32); next.length = total;
  mulib_ct_blob_free (out); *out = next; return 0;
}
int mulib_container_packcoff_open (const void *file, size_t n, size_t max, const unsigned char **image, size_t *length, mulib_imgformat_view *out)
{
  const unsigned char *p = (const unsigned char *)file, *found = NULL;
  mulib_coff_t v;
  mulib_imgformat_view view;
  size_t i, bytes = 0;
  int rc;
  if (!image || !length || !out) return MULIB_CT_EINVAL;
  if (mulib_coff_open (&v, file, n)) return MULIB_CT_EFORMAT;
  if (n >= 32 && !memcmp (p + n - 32, "MCOFPK1\0", 8)) {
    const unsigned char *t = p + n - 32;
    uint64_t off = mulib_ct_load64 (t + 8), size = mulib_ct_load64 (t + 16);
    if (off > n - 32 || size != n - 32 - off) return MULIB_CT_EFORMAT;
    if (mulib_crc32 (t, 28) != mulib_ct_load32 (t + 28)) return MULIB_CT_ECHECKSUM;
    if (size > max) return MULIB_CT_ELIMIT;
    found = p + (size_t)off; bytes = (size_t)size;
    if (mulib_crc32 (found, bytes) != mulib_ct_load32 (t + 24)) return MULIB_CT_ECHECKSUM;
    if (mulib_coff_open (&v, file, (size_t)off)) return MULIB_CT_EFORMAT;
  } else for (i = 0; i < v.section_count; ++i) {
    const char *name; size_t len;
    mulib_coff_section_t s;
    if (mulib_coff_section_name (&v, i, &name, &len)) return MULIB_CT_EFORMAT;
    if (len != 6 || memcmp (name, ".mulib", 6)) continue;
    if (found || mulib_coff_section (&v, i, &s) || !s.offset || (s.flags & 0x80)) return MULIB_CT_EFORMAT;
    if (s.size > max) return MULIB_CT_ELIMIT;
    if (mulib_coff_bytes (&v, s.offset, s.size, &found)) return MULIB_CT_EFORMAT;
    bytes = s.size;
  }
  if (!found) return MULIB_CT_ENOENT;
  rc = mulib_imgformat_open (found, bytes, max, &view); if (rc) return rc;
  *image = found; *length = bytes; *out = view; return 0;
}
#endif
