/** @file mulib-macho.h
 * @brief Checked thin Mach-O 32/64-bit views in either byte order.
 * Define MULIB_IMPL_MACHO and ENDIAN. No fat files, loading, dyld fixups or
 * signature verification. Unknown load commands remain bounded opaque records. */
#ifndef MULIB_MACHO_H
#define MULIB_MACHO_H
#include "mulib-endian.h"
#ifdef __cplusplus
extern "C" {
#endif
enum mulib_macho_result {
  MULIB_MACHO_OK = 0, MULIB_MACHO_EINVAL = -1, MULIB_MACHO_EFORMAT = -2,
  MULIB_MACHO_EBOUNDS = -3, MULIB_MACHO_EUNSUPPORTED = -4, MULIB_MACHO_ENOTFOUND = -5
};
/** @brief Borrowed validated view; do not modify fields or backing bytes. */
typedef struct mulib_macho {
  const unsigned char *data;
  size_t size, header_size, command_count, command_size, section_count;
  size_t symbol_offset, symbol_count, string_offset, string_size;
  unsigned bits, little_endian;
  uint32_t cpu, subtype, type, flags;
} mulib_macho_t;
/** @brief Borrowed bounded command bytes, including the eight-byte header. */
typedef struct mulib_macho_command {
  uint32_t type, size;
  const unsigned char *data;
} mulib_macho_command_t;
/** @brief Names are copied and NUL-terminated; size is virtual for zero-fill. */
typedef struct mulib_macho_section {
  char name[17], segment[17];
  uint64_t address, size;
  uint32_t offset, alignment, relocation_offset, relocation_count, flags;
} mulib_macho_section_t;
typedef struct mulib_macho_symbol {
  uint32_t name;
  unsigned type, section_index;
  uint16_t description;
  uint64_t value;
} mulib_macho_symbol_t;
/** @brief Validate command structure, segments, sections and symbol/string ranges. */
int mulib_macho_open (mulib_macho_t *out, const void *data, size_t size);
/** @brief Decode a zero-based load command. */
int mulib_macho_command (const mulib_macho_t *v, size_t index, mulib_macho_command_t *out);
/** @brief Decode a zero-based section across all segment commands. */
int mulib_macho_section (const mulib_macho_t *v, size_t index, mulib_macho_section_t *out);
/** @brief Borrow the section name; length may be sixteen, no terminating NUL required. */
int mulib_macho_section_name (const mulib_macho_t *v, size_t index, const char **out, size_t *length);
/** @brief Decode a zero-based nlist/nlist_64 symbol. */
int mulib_macho_symbol (const mulib_macho_t *v, size_t index, mulib_macho_symbol_t *out);
/** @brief Borrow a bounded NUL-terminated symbol name. */
int mulib_macho_symbol_name (const mulib_macho_t *v, size_t index, const char **out, size_t *length);
/** @brief Borrow checked file bytes; zero length at EOF is valid. */
int mulib_macho_bytes (const mulib_macho_t *v, uint64_t offset, uint64_t size, const unsigned char **out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MACHO) && !defined(MULIB_MACHO_IMPLEMENTED)
#define MULIB_MACHO_IMPLEMENTED
#include <string.h>
static uint32_t mulib_macho_u32_ (const mulib_macho_t *v, const unsigned char *p)
{ return v->little_endian ? mulib_endian_load_le32 (p) : mulib_endian_load_be32 (p); }
static uint64_t mulib_macho_u64_ (const mulib_macho_t *v, const unsigned char *p)
{
  uint64_t a = mulib_macho_u32_ (v, p), b = mulib_macho_u32_ (v, p + 4);
  return v->little_endian ? a | b << 32 : a << 32 | b;
}
int mulib_macho_bytes (const mulib_macho_t *v, uint64_t off, uint64_t n, const unsigned char **out)
{
  if (!v || !v->data || !out) return MULIB_MACHO_EINVAL;
  if (off > v->size || n > v->size - (size_t)off) return MULIB_MACHO_EBOUNDS;
  *out = v->data + (size_t)off; return 0;
}
int mulib_macho_command (const mulib_macho_t *v, size_t i, mulib_macho_command_t *out)
{
  size_t at, j, end;
  mulib_macho_command_t c;
  if (!v || !v->data || !out) return MULIB_MACHO_EINVAL;
  if (i >= v->command_count || v->header_size > v->size
      || v->command_size > v->size - v->header_size) return MULIB_MACHO_EBOUNDS;
  at = v->header_size; end = at + v->command_size;
  for (j = 0; j <= i; ++j) {
    if (end - at < 8) return MULIB_MACHO_EBOUNDS;
    c.data = v->data + at; c.type = mulib_macho_u32_ (v, c.data);
    c.size = mulib_macho_u32_ (v, c.data + 4);
    if (c.size < 8 || c.size > end - at || c.size % (v->bits == 64 ? 8 : 4)) return MULIB_MACHO_EFORMAT;
    at += c.size;
  }
  *out = c; return 0;
}
static int mulib_macho_section_ptr_ (const mulib_macho_t *v, size_t i, const unsigned char **out)
{
  size_t j, at = 0;
  if (!v || !v->data || !out) return MULIB_MACHO_EINVAL;
  if (i >= v->section_count) return MULIB_MACHO_EBOUNDS;
  for (j = 0; j < v->command_count; ++j) {
    mulib_macho_command_t c;
    int rc = mulib_macho_command (v, j, &c); if (rc) return rc;
    if (c.type == 1 || c.type == 0x19) {
      size_t base = c.type == 1 ? 56 : 72, stride = c.type == 1 ? 68 : 80, count;
      if (c.size < base) return MULIB_MACHO_EFORMAT;
      count = mulib_macho_u32_ (v, c.data + base - 8);
      if (count > (c.size - base) / stride) return MULIB_MACHO_EBOUNDS;
      if (i - at < count) { *out = c.data + base + (i - at) * stride; return 0; }
      at += count;
    }
  }
  return MULIB_MACHO_EBOUNDS;
}
static int mulib_macho_section_decode_ (const mulib_macho_t *v, const unsigned char *p, mulib_macho_section_t *out)
{
  const unsigned char *unused;
  mulib_macho_section_t s;
  unsigned type;
  memset (&s, 0, sizeof (s)); memcpy (s.name, p, 16); memcpy (s.segment, p + 16, 16);
  s.address = v->bits == 64 ? mulib_macho_u64_ (v, p + 32) : mulib_macho_u32_ (v, p + 32);
  s.size = v->bits == 64 ? mulib_macho_u64_ (v, p + 40) : mulib_macho_u32_ (v, p + 36);
  p += v->bits == 64 ? 48 : 40;
  s.offset = mulib_macho_u32_ (v, p); s.alignment = mulib_macho_u32_ (v, p + 4);
  s.relocation_offset = mulib_macho_u32_ (v, p + 8); s.relocation_count = mulib_macho_u32_ (v, p + 12);
  s.flags = mulib_macho_u32_ (v, p + 16); type = s.flags & 255u;
  if (s.alignment >= (v->bits == 64 ? 64u : 32u) || s.address > UINT64_MAX - s.size) return MULIB_MACHO_EFORMAT;
  if ((type != 1 && type != 0xc && type != 0x12 && mulib_macho_bytes (v, s.offset, s.size, &unused))
      || (s.relocation_count && mulib_macho_bytes (v, s.relocation_offset, (uint64_t)s.relocation_count * 8, &unused)))
    return MULIB_MACHO_EBOUNDS;
  *out = s; return 0;
}
int mulib_macho_section (const mulib_macho_t *v, size_t i, mulib_macho_section_t *out)
{
  const unsigned char *p;
  int rc;
  if (!out) return MULIB_MACHO_EINVAL;
  rc = mulib_macho_section_ptr_ (v, i, &p); if (rc) return rc;
  return mulib_macho_section_decode_ (v, p, out);
}
int mulib_macho_section_name (const mulib_macho_t *v, size_t i, const char **out, size_t *n)
{
  const unsigned char *p, *end;
  int rc;
  if (!out || !n) return MULIB_MACHO_EINVAL;
  rc = mulib_macho_section_ptr_ (v, i, &p); if (rc) return rc;
  end = (const unsigned char *)memchr (p, 0, 16);
  *out = (const char *)p; *n = end ? (size_t)(end - p) : 16; return 0;
}
int mulib_macho_symbol (const mulib_macho_t *v, size_t i, mulib_macho_symbol_t *out)
{
  const unsigned char *p;
  mulib_macho_symbol_t s;
  size_t stride;
  if (!v || !v->data || !out) return MULIB_MACHO_EINVAL;
  stride = v->bits == 64 ? 16 : 12;
  if (i >= v->symbol_count || v->symbol_offset > v->size
      || v->symbol_count > (v->size - v->symbol_offset) / stride) return MULIB_MACHO_EBOUNDS;
  p = v->data + v->symbol_offset + i * stride;
  s.name = mulib_macho_u32_ (v, p); s.type = p[4]; s.section_index = p[5];
  s.description = v->little_endian ? (uint16_t)((unsigned)p[6] | (unsigned)p[7] << 8)
      : (uint16_t)((unsigned)p[7] | (unsigned)p[6] << 8);
  s.value = v->bits == 64 ? mulib_macho_u64_ (v, p + 8) : mulib_macho_u32_ (v, p + 8);
  if (!(s.type & 0xe0u) && (s.type & 0x0eu) == 0x0eu
      && (!s.section_index || s.section_index > v->section_count)) return MULIB_MACHO_EFORMAT;
  *out = s; return 0;
}
int mulib_macho_symbol_name (const mulib_macho_t *v, size_t i, const char **out, size_t *n)
{
  mulib_macho_symbol_t s;
  const unsigned char *p, *end;
  int rc;
  if (!out || !n) return MULIB_MACHO_EINVAL;
  rc = mulib_macho_symbol (v, i, &s); if (rc) return rc;
  if (s.name >= v->string_size) return MULIB_MACHO_EFORMAT;
  p = v->data + v->string_offset + s.name;
  end = (const unsigned char *)memchr (p, 0, v->string_size - s.name);
  if (!end) return MULIB_MACHO_EFORMAT;
  *out = (const char *)p; *n = (size_t)(end - p); return 0;
}
int mulib_macho_open (mulib_macho_t *out, const void *data, size_t n)
{
  const unsigned char *p = (const unsigned char *)data, *unused;
  mulib_macho_t v;
  size_t i, used = 0;
  uint32_t magic;
  int symtab = 0;
  if (!out || !p) return MULIB_MACHO_EINVAL;
  if (n < 4) return MULIB_MACHO_EBOUNDS;
  magic = mulib_endian_load_le32 (p);
  if (magic != 0xfeedfaceu && magic != 0xfeedfacfu && magic != 0xcefaedfeu && magic != 0xcffaedfeu)
    return MULIB_MACHO_EUNSUPPORTED;
  memset (&v, 0, sizeof (v)); v.data = p; v.size = n;
  v.little_endian = magic == 0xfeedfaceu || magic == 0xfeedfacfu;
  v.bits = magic == 0xfeedfacfu || magic == 0xcffaedfeu ? 64u : 32u;
  v.header_size = v.bits == 64 ? 32 : 28;
  if (n < v.header_size) return MULIB_MACHO_EBOUNDS;
  v.cpu = mulib_macho_u32_ (&v, p + 4); v.subtype = mulib_macho_u32_ (&v, p + 8);
  v.type = mulib_macho_u32_ (&v, p + 12); v.command_count = mulib_macho_u32_ (&v, p + 16);
  v.command_size = mulib_macho_u32_ (&v, p + 20); v.flags = mulib_macho_u32_ (&v, p + 24);
  if (v.command_size > n - v.header_size || v.command_count > v.command_size / 8) return MULIB_MACHO_EBOUNDS;
  for (i = 0; i < v.command_count; ++i) {
    mulib_macho_command_t c;
    if (v.command_size - used < 8) return MULIB_MACHO_EBOUNDS;
    c.data = v.data + v.header_size + used;
    c.type = mulib_macho_u32_ (&v, c.data); c.size = mulib_macho_u32_ (&v, c.data + 4);
    if (c.size < 8 || c.size > v.command_size - used || c.size % (v.bits == 64 ? 8 : 4)) return MULIB_MACHO_EFORMAT;
    used += c.size;
    if (c.type == 1 || c.type == 0x19) {
      size_t base = c.type == 1 ? 56 : 72, stride = c.type == 1 ? 68 : 80, count, j;
      uint64_t off, bytes, memory;
      if ((c.type == 1) != (v.bits == 32) || c.size < base) return MULIB_MACHO_EFORMAT;
      count = mulib_macho_u32_ (&v, c.data + base - 8);
      if (count != (c.size - base) / stride || (c.size - base) % stride
          || count > SIZE_MAX - v.section_count) return MULIB_MACHO_EFORMAT;
      v.section_count += count;
      off = v.bits == 64 ? mulib_macho_u64_ (&v, c.data + 40) : mulib_macho_u32_ (&v, c.data + 32);
      bytes = v.bits == 64 ? mulib_macho_u64_ (&v, c.data + 48) : mulib_macho_u32_ (&v, c.data + 36);
      memory = v.bits == 64 ? mulib_macho_u64_ (&v, c.data + 32) : mulib_macho_u32_ (&v, c.data + 28);
      if (bytes > memory || mulib_macho_bytes (&v, off, bytes, &unused)) return MULIB_MACHO_EBOUNDS;
      for (j = 0; j < count; ++j) {
        mulib_macho_section_t s;
        int rc = mulib_macho_section_decode_ (&v, c.data + base + j * stride, &s);
        if (rc) return rc;
      }
    } else if (c.type == 2) {
      size_t stride = v.bits == 64 ? 16 : 12;
      if (symtab++ || c.size != 24) return MULIB_MACHO_EFORMAT;
      v.symbol_offset = mulib_macho_u32_ (&v, c.data + 8); v.symbol_count = mulib_macho_u32_ (&v, c.data + 12);
      v.string_offset = mulib_macho_u32_ (&v, c.data + 16); v.string_size = mulib_macho_u32_ (&v, c.data + 20);
      if (v.symbol_offset > n || v.symbol_count > (n - v.symbol_offset) / stride
          || mulib_macho_bytes (&v, v.string_offset, v.string_size, &unused)) return MULIB_MACHO_EBOUNDS;
    }
  }
  if (used != v.command_size) return MULIB_MACHO_EFORMAT;
  *out = v; return 0;
}
#endif
