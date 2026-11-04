/** @file mulib-container-packpe.h
 * @brief PE32+ data-image packing and non-destructive unsigned PE overlays.
 * Define MULIB_IMPL_CONTAINER_PACKPE plus IMGFORMAT/PE dependencies. */
#ifndef MULIB_CONTAINER_PACKPE_H
#define MULIB_CONTAINER_PACKPE_H
#include "mulib-imgformat.h"
#include "mulib-pe.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Construct AMD64 (machine 0x8664) PE32+ data-only DLL, no entry point,
 * imports or signature. Unlike COFF this is an image, not a relocatable object. */
int mulib_container_packpe_object (const void *image, size_t length, uint16_t machine, mulib_ct_blob *out, size_t limit);
/** @brief Append image/CRC trailer, preserving bytes. Reject certificate directories;
 * not a signing API and does not repair checksums or runtime metadata. */
int mulib_container_packpe_append (const void *file, size_t file_length, const void *image, size_t image_length, mulib_ct_blob *out, size_t limit);
/** @brief Borrow validated image from trailer or unique .mulib section, excluding padding. */
int mulib_container_packpe_open (const void *file, size_t length, size_t max_image, const unsigned char **image, size_t *image_length, mulib_imgformat_view *view);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINER_PACKPE) && !defined(MULIB_CONTAINER_PACKPE_IMPLEMENTED)
#define MULIB_CONTAINER_PACKPE_IMPLEMENTED
int mulib_container_packpe_object (const void *image, size_t n, uint16_t machine, mulib_ct_blob *out, size_t limit)
{
  mulib_ct_blob next = { 0 };
  mulib_imgformat_view view;
  unsigned char *p;
  size_t raw, total;
  int rc;
  if (!out) return MULIB_CT_EINVAL;
  if (machine != 0x8664) return MULIB_CT_ENOTSUP;
  rc = mulib_imgformat_open (image, n, limit, &view); if (rc) return rc;
  if (n > UINT32_MAX - 8192 || n > SIZE_MAX - 511) return MULIB_CT_ELIMIT;
  raw = (n + 511) & ~(size_t)511;
  if (mulib_ct_add_size (512, raw, &total)) return MULIB_CT_ELIMIT;
  rc = mulib_ct_blob_reserve (&next, total, limit); if (rc) return rc;
  p = next.data; memset (p, 0, total); next.length = total;
  memcpy (p, "MZ", 2); mulib_ct_store32 (p + 60, 128); memcpy (p + 128, "PE\0\0", 4);
  p[132] = 0x64; p[133] = 0x86; p[134] = 1; p[148] = 240; p[150] = 0x23; p[151] = 0x20;
  p[152] = 0x0b; p[153] = 2; mulib_ct_store32 (p + 160, (uint32_t)raw);
  mulib_ct_store64 (p + 176, UINT64_C (0x180000000)); mulib_ct_store32 (p + 184, 4096);
  mulib_ct_store32 (p + 188, 512); p[192] = 6; p[200] = 6;
  mulib_ct_store32 (p + 208, (uint32_t)(((n + 4095) & ~(size_t)4095) + 4096));
  mulib_ct_store32 (p + 212, 512); p[220] = 3; mulib_ct_store32 (p + 260, 16);
  memcpy (p + 392, ".mulib", 6); mulib_ct_store32 (p + 400, (uint32_t)n);
  mulib_ct_store32 (p + 404, 4096); mulib_ct_store32 (p + 408, (uint32_t)raw);
  mulib_ct_store32 (p + 412, 512); mulib_ct_store32 (p + 428, 0x40000040);
  memcpy (p + 512, image, n); mulib_ct_blob_free (out); *out = next; return 0;
}
int mulib_container_packpe_append (const void *file, size_t file_n, const void *image, size_t image_n, mulib_ct_blob *out, size_t limit)
{
  mulib_pe_t v;
  mulib_imgformat_view view;
  mulib_ct_blob next = { 0 };
  unsigned char t[32] = { 0 };
  size_t total;
  int rc;
  if (!out) return MULIB_CT_EINVAL;
  if (mulib_pe_open (&v, file, file_n)) return MULIB_CT_EFORMAT;
  if (v.directory_count > 4) {
    mulib_pe_directory_t d;
    if (mulib_pe_directory (&v, 4, &d)) return MULIB_CT_EFORMAT;
    if (d.size) return MULIB_CT_ENOTSUP;
  }
  rc = mulib_imgformat_open (image, image_n, limit, &view); if (rc) return rc;
  if (mulib_ct_add_size (file_n, image_n, &total) || mulib_ct_add_size (total, 32, &total)) return MULIB_CT_ELIMIT;
  rc = mulib_ct_blob_reserve (&next, total, limit); if (rc) return rc;
  memcpy (t, "MPEFPK1", 7); mulib_ct_store64 (t + 8, file_n); mulib_ct_store64 (t + 16, image_n);
  mulib_ct_store32 (t + 24, mulib_crc32 (image, image_n)); mulib_ct_store32 (t + 28, mulib_crc32 (t, 28));
  memcpy (next.data, file, file_n); memcpy (next.data + file_n, image, image_n);
  memcpy (next.data + file_n + image_n, t, 32); next.length = total;
  mulib_ct_blob_free (out); *out = next; return 0;
}
int mulib_container_packpe_open (const void *file, size_t n, size_t max, const unsigned char **image, size_t *length, mulib_imgformat_view *out)
{
  const unsigned char *p = (const unsigned char *)file, *found = NULL;
  mulib_pe_t v;
  mulib_imgformat_view view;
  size_t i, bytes = 0;
  int rc;
  if (!image || !length || !out) return MULIB_CT_EINVAL;
  if (mulib_pe_open (&v, file, n)) return MULIB_CT_EFORMAT;
  if (n >= 32 && !memcmp (p + n - 32, "MPEFPK1\0", 8)) {
    const unsigned char *t = p + n - 32;
    uint64_t off = mulib_ct_load64 (t + 8), size = mulib_ct_load64 (t + 16);
    if (off > n - 32 || size != n - 32 - off) return MULIB_CT_EFORMAT;
    if (mulib_crc32 (t, 28) != mulib_ct_load32 (t + 28)) return MULIB_CT_ECHECKSUM;
    if (size > max) return MULIB_CT_ELIMIT;
    found = p + (size_t)off; bytes = (size_t)size;
    if (mulib_crc32 (found, bytes) != mulib_ct_load32 (t + 24)) return MULIB_CT_ECHECKSUM;
    if (mulib_pe_open (&v, file, (size_t)off)) return MULIB_CT_EFORMAT;
  } else for (i = 0; i < v.coff.section_count; ++i) {
    const char *name; size_t len;
    mulib_pe_section_t s;
    if (mulib_pe_section_name (&v, i, &name, &len)) return MULIB_CT_EFORMAT;
    if (len != 6 || memcmp (name, ".mulib", 6)) continue;
    if (found || mulib_pe_section (&v, i, &s) || !s.offset || s.virtual_size > s.size) return MULIB_CT_EFORMAT;
    if (s.virtual_size > max) return MULIB_CT_ELIMIT;
    if (mulib_coff_bytes (&v.coff, s.offset, s.virtual_size, &found)) return MULIB_CT_EFORMAT;
    bytes = s.virtual_size;
  }
  if (!found) return MULIB_CT_ENOENT;
  rc = mulib_imgformat_open (found, bytes, max, &view); if (rc) return rc;
  *image = found; *length = bytes; *out = view; return 0;
}
#endif
