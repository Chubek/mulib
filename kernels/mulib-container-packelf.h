/** @file mulib-container-packelf.h
 * @brief ELF64 image objects and non-destructive ELF executable image overlays.
 * Define MULIB_IMPL_CONTAINER_PACKELF plus ELF/IMGFORMAT dependencies. */
#ifndef MULIB_CONTAINER_PACKELF_H
#define MULIB_CONTAINER_PACKELF_H
#include "mulib-imgformat.h"
#include "mulib-elf.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Construct a deterministic ELF64 little-endian relocatable object
 * containing .mulib.image and .shstrtab. machine is the native ELF e_machine. */
int mulib_container_packelf_object (const void *image, size_t length, uint16_t machine,
                                    mulib_ct_blob *out, size_t limit);
/** @brief Append image bytes and a checked 32-byte locator trailer to a validated
 * ELF file. Original ELF bytes, entry point, segments and sections are preserved. */
int mulib_container_packelf_append (const void *elf, size_t elf_length,
    const void *image, size_t image_length, mulib_ct_blob *out, size_t limit);
/** @brief Find one image in a .mulib.image section or the final overlay trailer.
 * Verify ELF bounds, locator checksum, and image envelope. Output borrows input. */
int mulib_container_packelf_open (const void *elf, size_t length, size_t max_image,
    const unsigned char **image, size_t *image_length, mulib_imgformat_view *view);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINER_PACKELF) && !defined(MULIB_CONTAINER_PACKELF_IMPLEMENTED)
#define MULIB_CONTAINER_PACKELF_IMPLEMENTED
static void mulib_container_packelf_u16_ (unsigned char *p, uint16_t n)
{ p[0] = (unsigned char)n; p[1] = (unsigned char)(n >> 8); }
int
mulib_container_packelf_object (const void *image, size_t n, uint16_t machine,
                                mulib_ct_blob *out, size_t limit)
{
  static const char names[] = "\0.shstrtab\0.mulib.image\0";
  mulib_ct_blob next = { NULL, 0, 0 };
  mulib_imgformat_view view;
  size_t data_offset = (64 + sizeof (names) + 7) & ~(size_t)7, section_offset, total;
  unsigned char *p, *s;
  int rc;
  if (!out) return MULIB_CT_EINVAL;
  rc = mulib_imgformat_open (image, n, limit, &view);
  if (rc) return rc;
  if (mulib_ct_add_size (data_offset, n, &section_offset)
      || section_offset > SIZE_MAX - 7) return MULIB_CT_ELIMIT;
  section_offset = (section_offset + 7) & ~(size_t)7;
  if (mulib_ct_add_size (section_offset, 192, &total)) return MULIB_CT_ELIMIT;
  rc = mulib_ct_blob_reserve (&next, total, limit);
  if (rc) return rc;
  p = next.data; memset (p, 0, total); next.length = total;
  memcpy (p, "\177ELF", 4); p[4] = 2; p[5] = 1; p[6] = 1;
  mulib_container_packelf_u16_ (p + 16, 1); mulib_container_packelf_u16_ (p + 18, machine);
  mulib_ct_store32 (p + 20, 1); mulib_ct_store64 (p + 40, section_offset);
  mulib_container_packelf_u16_ (p + 52, 64); mulib_container_packelf_u16_ (p + 58, 64);
  mulib_container_packelf_u16_ (p + 60, 3); mulib_container_packelf_u16_ (p + 62, 1);
  memcpy (p + 64, names, sizeof (names)); memcpy (p + data_offset, image, n);
  s = p + section_offset + 64;
  mulib_ct_store32 (s, 1); mulib_ct_store32 (s + 4, 3);
  mulib_ct_store64 (s + 24, 64); mulib_ct_store64 (s + 32, sizeof (names)); mulib_ct_store64 (s + 48, 1);
  s += 64; mulib_ct_store32 (s, 1 + (uint32_t)sizeof (".shstrtab")); mulib_ct_store32 (s + 4, 1);
  mulib_ct_store64 (s + 24, data_offset); mulib_ct_store64 (s + 32, n); mulib_ct_store64 (s + 48, 8);
  mulib_ct_blob_free (out); *out = next;
  return 0;
}
int
mulib_container_packelf_append (const void *elf, size_t elf_n, const void *image,
    size_t image_n, mulib_ct_blob *out, size_t limit)
{
  mulib_elf_t file;
  mulib_imgformat_view view;
  mulib_ct_blob next = { NULL, 0, 0 };
  unsigned char trailer[32] = { 0 };
  size_t total;
  int rc;
  if (!out || mulib_elf_open (&file, elf, elf_n)) return MULIB_CT_EFORMAT;
  rc = mulib_imgformat_open (image, image_n, limit, &view);
  if (rc) return rc;
  if (mulib_ct_add_size (elf_n, image_n, &total)
      || mulib_ct_add_size (total, 32, &total)) return MULIB_CT_ELIMIT;
  rc = mulib_ct_blob_reserve (&next, total, limit);
  if (rc) return rc;
  memcpy (trailer, "MELFPK1", 7); mulib_ct_store64 (trailer + 8, elf_n);
  mulib_ct_store64 (trailer + 16, image_n);
  mulib_ct_store32 (trailer + 24, mulib_crc32 (image, image_n));
  mulib_ct_store32 (trailer + 28, mulib_crc32 (trailer, 28));
  memcpy (next.data, elf, elf_n); memcpy (next.data + elf_n, image, image_n);
  memcpy (next.data + elf_n + image_n, trailer, 32); next.length = total;
  mulib_ct_blob_free (out); *out = next;
  return 0;
}
int
mulib_container_packelf_open (const void *data, size_t n, size_t max,
    const unsigned char **image, size_t *length, mulib_imgformat_view *out)
{
  const unsigned char *p = (const unsigned char *)data, *found = NULL;
  mulib_elf_t elf;
  mulib_imgformat_view view;
  size_t i, bytes = 0;
  int rc;
  if (!image || !length || !out || mulib_elf_open (&elf, data, n)) return MULIB_CT_EFORMAT;
  if (n >= 32 && !memcmp (p + n - 32, "MELFPK1\0", 8))
    {
      const unsigned char *t = p + n - 32;
      uint64_t offset = mulib_ct_load64 (t + 8), size = mulib_ct_load64 (t + 16);
      if (offset > n - 32 || size != n - 32 - offset) return MULIB_CT_EFORMAT;
      if (mulib_crc32 (t, 28) != mulib_ct_load32 (t + 28)) return MULIB_CT_ECHECKSUM;
      found = p + (size_t)offset; bytes = (size_t)size;
      if (bytes > max) return MULIB_CT_ELIMIT;
      if (mulib_crc32 (found, bytes) != mulib_ct_load32 (t + 24)) return MULIB_CT_ECHECKSUM;
      if (mulib_elf_open (&elf, data, (size_t)offset)) return MULIB_CT_EFORMAT;
    }
  else
    for (i = 0; i < elf.shnum; ++i)
      {
        const char *name;
        size_t name_n;
        mulib_elf_section_t section;
        rc = mulib_elf_section_name (&elf, i, &name, &name_n);
        if (rc == MULIB_ELF_ENOTFOUND) continue;
        if (rc) return MULIB_CT_EFORMAT;
        if (name_n != 12 || memcmp (name, ".mulib.image", 12)) continue;
        if (found || mulib_elf_section (&elf, i, &section) || section.type != 1)
          return MULIB_CT_EFORMAT;
        if (section.size > max) return MULIB_CT_ELIMIT;
        if (mulib_elf_bytes (&elf, section.offset, section.size, &found)) return MULIB_CT_EFORMAT;
        bytes = (size_t)section.size;
      }
  if (!found) return MULIB_CT_ENOENT;
  rc = mulib_imgformat_open (found, bytes, max, &view);
  if (rc) return rc;
  *image = found; *length = bytes; *out = view;
  return 0;
}
#endif
