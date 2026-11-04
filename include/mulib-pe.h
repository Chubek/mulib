/** @file mulib-pe.h
 * @brief Checked PE32/PE32+ headers, sections, directories and RVA mapping.
 * Define MULIB_IMPL_PE, COFF and ENDIAN. No loading or import/export decoding. */
#ifndef MULIB_PE_H
#define MULIB_PE_H
#include "mulib-coff.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief PE-specific spellings of the shared COFF structural results. */
enum mulib_pe_result {
  MULIB_PE_OK = MULIB_COFF_OK, MULIB_PE_EINVAL = MULIB_COFF_EINVAL,
  MULIB_PE_EFORMAT = MULIB_COFF_EFORMAT, MULIB_PE_EBOUNDS = MULIB_COFF_EBOUNDS,
  MULIB_PE_EUNSUPPORTED = MULIB_COFF_EUNSUPPORTED, MULIB_PE_ENOTFOUND = MULIB_COFF_ENOTFOUND
};
/** @brief Borrowed PE view; COFF result codes apply. */
typedef struct mulib_pe {
  mulib_coff_t coff;
  unsigned bits;
  uint64_t image_base;
  uint32_t entry, image_size, headers_size, section_alignment, file_alignment;
  uint16_t subsystem;
  size_t directory_offset, directory_count;
} mulib_pe_t;
typedef mulib_coff_section_t mulib_pe_section_t;
typedef mulib_coff_symbol_t mulib_pe_symbol_t;
/** @brief Directory address is an RVA except certificate index 4 (file offset). */
typedef struct mulib_pe_directory { uint32_t address, size; } mulib_pe_directory_t;
/** @brief Validate headers, section payloads and directory table; atomic output. */
int mulib_pe_open (mulib_pe_t *out, const void *data, size_t size);
/** @brief Decode zero-based section. */
int mulib_pe_section (const mulib_pe_t *v, size_t index, mulib_pe_section_t *out);
/** @brief Borrow bounded section name. */
int mulib_pe_section_name (const mulib_pe_t *v, size_t index, const char **name, size_t *length);
/** @brief Decode a primary COFF debug symbol (not an export) by raw index. */
int mulib_pe_symbol (const mulib_pe_t *v, size_t index, mulib_pe_symbol_t *out);
/** @brief Borrow a primary COFF debug-symbol name. */
int mulib_pe_symbol_name (const mulib_pe_t *v, size_t index, const char **name, size_t *length);
/** @brief Borrow checked file bytes by file offset, not RVA. */
int mulib_pe_bytes (const mulib_pe_t *v, uint64_t offset, uint64_t size, const unsigned char **out);
/** @brief Decode directory, including absent zero/zero entries. */
int mulib_pe_directory (const mulib_pe_t *v, size_t index, mulib_pe_directory_t *out);
/** @brief Map only file-backed RVA bytes, rejecting zero-fill and ambiguous overlaps. */
int mulib_pe_rva (const mulib_pe_t *v, uint32_t rva, size_t size, const unsigned char **out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PE) && !defined(MULIB_PE_IMPLEMENTED)
#define MULIB_PE_IMPLEMENTED
int mulib_pe_section (const mulib_pe_t *v, size_t i, mulib_pe_section_t *out)
{ return v ? mulib_coff_section (&v->coff, i, out) : MULIB_COFF_EINVAL; }
int mulib_pe_section_name (const mulib_pe_t *v, size_t i, const char **out, size_t *n)
{ return v ? mulib_coff_section_name (&v->coff, i, out, n) : MULIB_COFF_EINVAL; }
int mulib_pe_symbol (const mulib_pe_t *v, size_t i, mulib_pe_symbol_t *out)
{ return v ? mulib_coff_symbol (&v->coff, i, out) : MULIB_COFF_EINVAL; }
int mulib_pe_symbol_name (const mulib_pe_t *v, size_t i, const char **out, size_t *n)
{ return v ? mulib_coff_symbol_name (&v->coff, i, out, n) : MULIB_COFF_EINVAL; }
int mulib_pe_bytes (const mulib_pe_t *v, uint64_t off, uint64_t n, const unsigned char **out)
{ return v ? mulib_coff_bytes (&v->coff, off, n, out) : MULIB_COFF_EINVAL; }
int mulib_pe_directory (const mulib_pe_t *v, size_t i, mulib_pe_directory_t *out)
{
  const unsigned char *p;
  mulib_pe_directory_t d;
  if (!v || !out) return MULIB_COFF_EINVAL;
  if (i >= v->directory_count) return MULIB_COFF_EBOUNDS;
  if (mulib_coff_bytes (&v->coff, v->directory_offset + i * 8, 8, &p)) return MULIB_COFF_EBOUNDS;
  d.address = mulib_endian_load_le32 (p); d.size = mulib_endian_load_le32 (p + 4);
  if ((!d.address) != (!d.size)) return MULIB_COFF_EFORMAT;
  *out = d; return 0;
}
int mulib_pe_rva (const mulib_pe_t *v, uint32_t rva, size_t n, const unsigned char **out)
{
  const unsigned char *found = NULL, *p;
  mulib_pe_section_t s;
  size_t i;
  if (!v || !out) return MULIB_COFF_EINVAL;
  if (n > UINT64_C (0x100000000) - rva || n > v->image_size
      || rva > v->image_size - n) return MULIB_COFF_EBOUNDS;
  if (rva < v->headers_size && n <= v->headers_size - rva) {
    if (mulib_coff_bytes (&v->coff, rva, n, &found)) return MULIB_COFF_EBOUNDS;
  }
  for (i = 0; i < v->coff.section_count; ++i) {
    uint64_t delta, span;
    int rc = mulib_pe_section (v, i, &s); if (rc) return rc;
    span = s.virtual_size > s.size ? s.virtual_size : s.size;
    if (rva < s.address || (uint64_t)rva - s.address >= span) continue;
    delta = (uint64_t)rva - s.address;
    if (found) return MULIB_COFF_EFORMAT;
    if (!s.offset || delta > s.size || n > s.size - delta) return MULIB_COFF_EBOUNDS;
    if (mulib_coff_bytes (&v->coff, (uint64_t)s.offset + delta, n, &p)) return MULIB_COFF_EBOUNDS;
    found = p;
  }
  if (!found) return MULIB_COFF_EBOUNDS;
  *out = found; return 0;
}
int mulib_pe_open (mulib_pe_t *out, const void *data, size_t n)
{
  const unsigned char *p = (const unsigned char *)data;
  mulib_pe_t v;
  size_t off, base, i;
  uint32_t dirs;
  unsigned magic;
  int rc;
  if (!out || !p) return MULIB_COFF_EINVAL;
  if (n < 64 || p[0] != 'M' || p[1] != 'Z') return MULIB_COFF_EFORMAT;
  off = mulib_endian_load_le32 (p + 60);
  if (off < 64 || off > n || n - off < 24 || memcmp (p + off, "PE\0\0", 4)) return MULIB_COFF_EFORMAT;
  memset (&v, 0, sizeof (v));
  rc = mulib_coff_open_at (&v.coff, data, n, off + 4); if (rc) return rc;
  off += 24;
  if (v.coff.optional_size < 2) return MULIB_COFF_EFORMAT;
  p += off; magic = (unsigned)p[0] | (unsigned)p[1] << 8;
  if (magic != 0x10b && magic != 0x20b) return MULIB_COFF_EUNSUPPORTED;
  v.bits = magic == 0x10b ? 32u : 64u; base = v.bits == 32 ? 96 : 112;
  if (v.coff.optional_size < base) return MULIB_COFF_EBOUNDS;
  v.entry = mulib_endian_load_le32 (p + 16);
  v.image_base = v.bits == 32 ? mulib_endian_load_le32 (p + 28)
      : (uint64_t)mulib_endian_load_le32 (p + 24) | (uint64_t)mulib_endian_load_le32 (p + 28) << 32;
  v.section_alignment = mulib_endian_load_le32 (p + 32);
  v.file_alignment = mulib_endian_load_le32 (p + 36);
  v.image_size = mulib_endian_load_le32 (p + 56); v.headers_size = mulib_endian_load_le32 (p + 60);
  v.subsystem = (uint16_t)((unsigned)p[68] | (unsigned)p[69] << 8);
  dirs = mulib_endian_load_le32 (p + base - 4);
  if (dirs > (v.coff.optional_size - base) / 8) return MULIB_COFF_EBOUNDS;
  v.directory_offset = off + base; v.directory_count = dirs;
  if (!v.section_alignment || (v.section_alignment & (v.section_alignment - 1))
      || !v.file_alignment || (v.file_alignment & (v.file_alignment - 1))
      || v.section_alignment < v.file_alignment || v.headers_size > n
      || v.headers_size < v.coff.section_offset + v.coff.section_count * 40
      || v.image_size < v.headers_size || (v.entry && v.entry >= v.image_size)) return MULIB_COFF_EFORMAT;
  for (i = 0; i < v.coff.section_count; ++i) {
    mulib_pe_section_t s;
    uint64_t span;
    rc = mulib_pe_section (&v, i, &s); if (rc) return rc;
    span = s.virtual_size > s.size ? s.virtual_size : s.size;
    if (s.address > v.image_size || span > v.image_size - s.address
        || (s.size && !s.offset)) return MULIB_COFF_EFORMAT;
  }
  for (i = 0; i < v.directory_count; ++i) {
    mulib_pe_directory_t d;
    const unsigned char *unused;
    rc = mulib_pe_directory (&v, i, &d); if (rc) return rc;
    if (i == 4 && d.size && mulib_coff_bytes (&v.coff, d.address, d.size, &unused)) return MULIB_COFF_EBOUNDS;
  }
  *out = v; return 0;
}
#endif
