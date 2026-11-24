/** @file mulib-coff.h
 * @brief Checked little-endian Microsoft COFF sections, symbols and names.
 * Define MULIB_IMPL_COFF and MULIB_IMPL_ENDIAN. Not a loader; no bigobj,
 * archives, import objects or relocation-overflow records. */
#ifndef MULIB_COFF_H
#define MULIB_COFF_H
#include "mulib-endian.h"
#ifdef __cplusplus
extern "C" {
#endif
enum mulib_coff_result {
  MULIB_COFF_OK = 0, MULIB_COFF_EINVAL = -1, MULIB_COFF_EFORMAT = -2,
  MULIB_COFF_EBOUNDS = -3, MULIB_COFF_EUNSUPPORTED = -4,
  MULIB_COFF_ENOTFOUND = -5
};
/** @brief Borrowed validated tables; initialize with open, do not modify. */
typedef struct mulib_coff {
  const unsigned char *data;
  size_t size, header_offset, section_offset, section_count;
  size_t symbol_offset, symbol_count, string_offset, string_size;
  uint16_t machine, characteristics, optional_size;
  uint32_t timestamp;
} mulib_coff_t;
/** @brief Raw file size/offset differ from virtual size/address in PE images. */
typedef struct mulib_coff_section {
  uint32_t virtual_size, address, size, offset, relocation_offset;
  uint32_t line_offset, flags;
  uint16_t relocation_count, line_count;
} mulib_coff_section_t;
/** @brief Primary 18-byte symbol. Auxiliary entries are not symbols. */
typedef struct mulib_coff_symbol {
  uint32_t value;
  int32_t section_index;
  uint16_t type;
  unsigned storage_class, auxiliary_count;
} mulib_coff_symbol_t;
/** @brief Open a COFF file at offset zero; outputs unchanged on failure. */
int mulib_coff_open (mulib_coff_t *out, const void *data, size_t size);
/** @brief Open a COFF header inside a PE file; all offsets remain file-relative. */
int mulib_coff_open_at (mulib_coff_t *out, const void *data, size_t size, size_t offset);
/** @brief Decode a zero-based section and validate payload/relocation ranges. */
int mulib_coff_section (const mulib_coff_t *v, size_t index, mulib_coff_section_t *out);
/** @brief Borrow a section name (fixed eight bytes or /decimal string offset). */
int mulib_coff_section_name (const mulib_coff_t *v, size_t index, const char **name, size_t *length);
/** @brief Decode a primary symbol by raw table index; rejects auxiliary indexes. */
int mulib_coff_symbol (const mulib_coff_t *v, size_t index, mulib_coff_symbol_t *out);
/** @brief Borrow a primary symbol's short or string-table name. */
int mulib_coff_symbol_name (const mulib_coff_t *v, size_t index, const char **name, size_t *length);
/** @brief Borrow a checked file range, allowing empty one-past-end. */
int mulib_coff_bytes (const mulib_coff_t *v, uint64_t offset, uint64_t size, const unsigned char **out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_COFF) && !defined(MULIB_COFF_IMPLEMENTED)
#define MULIB_COFF_IMPLEMENTED
#include <string.h>
static uint16_t mulib_coff_u16_ (const unsigned char *p)
{ return (uint16_t)((uint16_t)p[0] | (uint16_t)p[1] << 8); }
int mulib_coff_bytes (const mulib_coff_t *v, uint64_t off, uint64_t n, const unsigned char **out)
{
  if (!v || !v->data || !out) return MULIB_COFF_EINVAL;
  if (off > v->size || n > v->size - (size_t)off) return MULIB_COFF_EBOUNDS;
  *out = v->data + (size_t)off; return 0;
}
int mulib_coff_section (const mulib_coff_t *v, size_t i, mulib_coff_section_t *out)
{
  const unsigned char *p, *unused;
  mulib_coff_section_t s;
  if (!v || !v->data || !out) return MULIB_COFF_EINVAL;
  if (i >= v->section_count || v->section_offset > v->size
      || v->section_count > (v->size - v->section_offset) / 40) return MULIB_COFF_EBOUNDS;
  p = v->data + v->section_offset + i * 40;
  s.virtual_size = mulib_endian_load_le32 (p + 8);
  s.address = mulib_endian_load_le32 (p + 12);
  s.size = mulib_endian_load_le32 (p + 16);
  s.offset = mulib_endian_load_le32 (p + 20);
  s.relocation_offset = mulib_endian_load_le32 (p + 24);
  s.line_offset = mulib_endian_load_le32 (p + 28);
  s.relocation_count = mulib_coff_u16_ (p + 32);
  s.line_count = mulib_coff_u16_ (p + 34);
  s.flags = mulib_endian_load_le32 (p + 36);
  if (s.flags & 0x01000000u) return MULIB_COFF_EUNSUPPORTED;
  if ((s.size && (s.offset || !(s.flags & 0x80u))
       && (!s.offset || mulib_coff_bytes (v, s.offset, s.size, &unused)))
      || (s.relocation_count && (!s.relocation_offset
          || mulib_coff_bytes (v, s.relocation_offset, (uint64_t)s.relocation_count * 10, &unused)))
      || (s.line_count && (!s.line_offset
          || mulib_coff_bytes (v, s.line_offset, (uint64_t)s.line_count * 6, &unused))))
    return MULIB_COFF_EBOUNDS;
  *out = s; return 0;
}
static int mulib_coff_string_ (const mulib_coff_t *v, size_t off, const char **out, size_t *n)
{
  const unsigned char *p, *end;
  if (off < 4 || off >= v->string_size) return MULIB_COFF_EFORMAT;
  p = v->data + v->string_offset + off;
  end = (const unsigned char *)memchr (p, 0, v->string_size - off);
  if (!end) return MULIB_COFF_EFORMAT;
  *out = (const char *)p; *n = (size_t)(end - p); return 0;
}
static void mulib_coff_short_ (const unsigned char *p, const char **out, size_t *n)
{
  const unsigned char *end = (const unsigned char *)memchr (p, 0, 8);
  *out = (const char *)p; *n = end ? (size_t)(end - p) : 8;
}
int mulib_coff_section_name (const mulib_coff_t *v, size_t i, const char **out, size_t *n)
{
  mulib_coff_section_t s;
  const unsigned char *p;
  size_t off = 0, j;
  int rc;
  if (!out || !n) return MULIB_COFF_EINVAL;
  rc = mulib_coff_section (v, i, &s); if (rc) return rc;
  p = v->data + v->section_offset + i * 40;
  if (*p != '/') { mulib_coff_short_ (p, out, n); return 0; }
  for (j = 1; j < 8 && p[j]; ++j) {
    if (p[j] < '0' || p[j] > '9' || off > (SIZE_MAX - (p[j] - '0')) / 10)
      return MULIB_COFF_EFORMAT;
    off = off * 10 + p[j] - '0';
  }
  return mulib_coff_string_ (v, off, out, n);
}
int mulib_coff_symbol (const mulib_coff_t *v, size_t i, mulib_coff_symbol_t *out)
{
  const unsigned char *p;
  size_t at = 0;
  uint16_t section;
  mulib_coff_symbol_t s;
  if (!v || !v->data || !out) return MULIB_COFF_EINVAL;
  if (i >= v->symbol_count || v->symbol_offset > v->size
      || v->symbol_count > (v->size - v->symbol_offset) / 18) return MULIB_COFF_EBOUNDS;
  while (at < i) {
    size_t step = 1u + v->data[v->symbol_offset + at * 18 + 17];
    if (step > v->symbol_count - at) return MULIB_COFF_EFORMAT;
    at += step;
  }
  if (at != i) return MULIB_COFF_EFORMAT;
  p = v->data + v->symbol_offset + i * 18;
  s.value = mulib_endian_load_le32 (p + 8);
  section = mulib_coff_u16_ (p + 12);
  s.section_index = section <= 32767u ? section : (int32_t)section - 65536;
  s.type = mulib_coff_u16_ (p + 14); s.storage_class = p[16]; s.auxiliary_count = p[17];
  if (s.auxiliary_count >= v->symbol_count - i
      || (s.section_index > 0 && (size_t)s.section_index > v->section_count)) return MULIB_COFF_EFORMAT;
  *out = s; return 0;
}
int mulib_coff_symbol_name (const mulib_coff_t *v, size_t i, const char **out, size_t *n)
{
  mulib_coff_symbol_t s;
  const unsigned char *p;
  int rc;
  if (!out || !n) return MULIB_COFF_EINVAL;
  rc = mulib_coff_symbol (v, i, &s); if (rc) return rc;
  p = v->data + v->symbol_offset + i * 18;
  if (!mulib_endian_load_le32 (p)) return mulib_coff_string_ (v, mulib_endian_load_le32 (p + 4), out, n);
  mulib_coff_short_ (p, out, n); return 0;
}
int mulib_coff_open_at (mulib_coff_t *out, const void *data, size_t n, size_t off)
{
  mulib_coff_t v;
  const unsigned char *p = (const unsigned char *)data;
  size_t i;
  mulib_coff_section_t s;
  if (!out || !p) return MULIB_COFF_EINVAL;
  if (off > n || n - off < 20) return MULIB_COFF_EBOUNDS;
  memset (&v, 0, sizeof (v)); v.data = p; v.size = n; v.header_offset = off;
  p += off; v.machine = mulib_coff_u16_ (p);
  if (!v.machine) return MULIB_COFF_EUNSUPPORTED;
  v.section_count = mulib_coff_u16_ (p + 2);
  v.timestamp = mulib_endian_load_le32 (p + 4);
  v.symbol_offset = mulib_endian_load_le32 (p + 8);
  v.symbol_count = mulib_endian_load_le32 (p + 12);
  v.optional_size = mulib_coff_u16_ (p + 16); v.characteristics = mulib_coff_u16_ (p + 18);
  if (v.optional_size > n - off - 20) return MULIB_COFF_EBOUNDS;
  v.section_offset = off + 20 + v.optional_size;
  if (v.section_count > (n - v.section_offset) / 40) return MULIB_COFF_EBOUNDS;
  if (v.symbol_count && !v.symbol_offset) return MULIB_COFF_EFORMAT;
  if (v.symbol_offset) {
    if (v.symbol_offset > n || v.symbol_count > (n - v.symbol_offset) / 18) return MULIB_COFF_EBOUNDS;
    v.string_offset = v.symbol_offset + v.symbol_count * 18;
    if (n - v.string_offset < 4) return MULIB_COFF_EBOUNDS;
    v.string_size = mulib_endian_load_le32 (v.data + v.string_offset);
    if (v.string_size < 4 || v.string_size > n - v.string_offset) return MULIB_COFF_EBOUNDS;
  }
  for (i = 0; i < v.section_count; ++i) {
    int rc = mulib_coff_section (&v, i, &s); if (rc) return rc;
  }
  for (i = 0; i < v.symbol_count;) {
    const unsigned char *q = v.data + v.symbol_offset + i * 18;
    if ((size_t)q[17] >= v.symbol_count - i) return MULIB_COFF_EFORMAT;
    i += 1u + q[17];
  }
  *out = v; return 0;
}
int mulib_coff_open (mulib_coff_t *out, const void *data, size_t n)
{ return mulib_coff_open_at (out, data, n, 0); }
#endif
