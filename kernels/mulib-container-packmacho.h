/** @file mulib-container-packmacho.h
 * @brief Mach-O64 image objects and non-destructive unsigned Mach-O overlays.
 * Define MULIB_IMPL_CONTAINER_PACKMACHO plus IMGFORMAT/MACHO dependencies. */
#ifndef MULIB_CONTAINER_PACKMACHO_H
#define MULIB_CONTAINER_PACKMACHO_H
#include "mulib-imgformat.h"
#include "mulib-macho.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Construct little-endian MH_OBJECT with __DATA,__mulib_image; native
 * x86-64 or arm64 CPU type, no symbols/relocations. Replace out atomically. */
int mulib_container_packmacho_object (const void *image, size_t length, uint32_t cpu, mulib_ct_blob *out, size_t limit);
/** @brief Preserve file bytes; reject LC_CODE_SIGNATURE. Appending is not signing. */
int mulib_container_packmacho_append (const void *file, size_t file_length, const void *image, size_t image_length, mulib_ct_blob *out, size_t limit);
/** @brief Borrow one validated image from trailer or __DATA,__mulib_image section. */
int mulib_container_packmacho_open (const void *file, size_t length, size_t max_image, const unsigned char **image, size_t *image_length, mulib_imgformat_view *view);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINER_PACKMACHO) && !defined(MULIB_CONTAINER_PACKMACHO_IMPLEMENTED)
#define MULIB_CONTAINER_PACKMACHO_IMPLEMENTED
int mulib_container_packmacho_object (const void *image, size_t n, uint32_t cpu, mulib_ct_blob *out, size_t limit)
{
  mulib_ct_blob next = { 0 };
  mulib_imgformat_view view;
  unsigned char *p;
  int rc;
  if (!out) return MULIB_CT_EINVAL;
  if (cpu != 0x01000007u && cpu != 0x0100000cu) return MULIB_CT_ENOTSUP;
  rc = mulib_imgformat_open (image, n, limit, &view); if (rc) return rc;
  if (n > UINT32_MAX - 192 || limit < 192 || n > limit - 192) return MULIB_CT_ELIMIT;
  rc = mulib_ct_blob_reserve (&next, n + 192, limit); if (rc) return rc;
  p = next.data; memset (p, 0, 192); next.length = n + 192;
  mulib_ct_store32 (p, 0xfeedfacf); mulib_ct_store32 (p + 4, cpu); mulib_ct_store32 (p + 8, cpu == 0x01000007u ? 3 : 0);
  mulib_ct_store32 (p + 12, 1); mulib_ct_store32 (p + 16, 1); mulib_ct_store32 (p + 20, 152);
  mulib_ct_store32 (p + 32, 0x19); mulib_ct_store32 (p + 36, 152);
  mulib_ct_store64 (p + 64, n); mulib_ct_store64 (p + 72, 192); mulib_ct_store64 (p + 80, n);
  mulib_ct_store32 (p + 88, 3); mulib_ct_store32 (p + 92, 3); mulib_ct_store32 (p + 96, 1);
  memcpy (p + 104, "__mulib_image", 13); memcpy (p + 120, "__DATA", 6);
  mulib_ct_store64 (p + 144, n); mulib_ct_store32 (p + 152, 192); mulib_ct_store32 (p + 156, 3);
  memcpy (p + 192, image, n); mulib_ct_blob_free (out); *out = next; return 0;
}
int mulib_container_packmacho_append (const void *file, size_t file_n, const void *image, size_t image_n, mulib_ct_blob *out, size_t limit)
{
  mulib_macho_t v;
  mulib_imgformat_view view;
  mulib_ct_blob next = { 0 };
  unsigned char t[32] = { 0 };
  size_t total, i;
  int rc;
  if (!out) return MULIB_CT_EINVAL;
  if (mulib_macho_open (&v, file, file_n)) return MULIB_CT_EFORMAT;
  for (i = 0; i < v.command_count; ++i) {
    mulib_macho_command_t c;
    if (mulib_macho_command (&v, i, &c)) return MULIB_CT_EFORMAT;
    if (c.type == 0x1d) return MULIB_CT_ENOTSUP;
  }
  rc = mulib_imgformat_open (image, image_n, limit, &view); if (rc) return rc;
  if (mulib_ct_add_size (file_n, image_n, &total) || mulib_ct_add_size (total, 32, &total)) return MULIB_CT_ELIMIT;
  rc = mulib_ct_blob_reserve (&next, total, limit); if (rc) return rc;
  memcpy (t, "MMACPK1", 7); mulib_ct_store64 (t + 8, file_n); mulib_ct_store64 (t + 16, image_n);
  mulib_ct_store32 (t + 24, mulib_crc32 (image, image_n)); mulib_ct_store32 (t + 28, mulib_crc32 (t, 28));
  memcpy (next.data, file, file_n); memcpy (next.data + file_n, image, image_n);
  memcpy (next.data + file_n + image_n, t, 32); next.length = total;
  mulib_ct_blob_free (out); *out = next; return 0;
}
int mulib_container_packmacho_open (const void *file, size_t n, size_t max, const unsigned char **image, size_t *length, mulib_imgformat_view *out)
{
  const unsigned char *p = (const unsigned char *)file, *found = NULL;
  mulib_macho_t v;
  mulib_imgformat_view view;
  size_t i, bytes = 0;
  int rc;
  if (!image || !length || !out) return MULIB_CT_EINVAL;
  if (mulib_macho_open (&v, file, n)) return MULIB_CT_EFORMAT;
  if (n >= 32 && !memcmp (p + n - 32, "MMACPK1\0", 8)) {
    const unsigned char *t = p + n - 32;
    uint64_t off = mulib_ct_load64 (t + 8), size = mulib_ct_load64 (t + 16);
    if (off > n - 32 || size != n - 32 - off) return MULIB_CT_EFORMAT;
    if (mulib_crc32 (t, 28) != mulib_ct_load32 (t + 28)) return MULIB_CT_ECHECKSUM;
    if (size > max) return MULIB_CT_ELIMIT;
    found = p + (size_t)off; bytes = (size_t)size;
    if (mulib_crc32 (found, bytes) != mulib_ct_load32 (t + 24)) return MULIB_CT_ECHECKSUM;
    if (mulib_macho_open (&v, file, (size_t)off)) return MULIB_CT_EFORMAT;
  } else for (i = 0; i < v.section_count; ++i) {
    mulib_macho_section_t s;
    unsigned type;
    if (mulib_macho_section (&v, i, &s)) return MULIB_CT_EFORMAT;
    if (strcmp (s.name, "__mulib_image") || strcmp (s.segment, "__DATA")) continue;
    type = s.flags & 255u;
    if (found || type == 1 || type == 0xc || type == 0x12) return MULIB_CT_EFORMAT;
    if (s.size > max) return MULIB_CT_ELIMIT;
    if (mulib_macho_bytes (&v, s.offset, s.size, &found)) return MULIB_CT_EFORMAT;
    bytes = (size_t)s.size;
  }
  if (!found) return MULIB_CT_ENOENT;
  rc = mulib_imgformat_open (found, bytes, max, &view); if (rc) return rc;
  *image = found; *length = bytes; *out = view; return 0;
}
#endif
