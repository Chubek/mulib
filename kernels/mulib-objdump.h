/** @file mulib-objdump.h
 * @brief Shared escaped object metadata and bounded binary hex dump rendering.
 * Define MULIB_IMPL_OBJDUMP, TERMSTYLE, TERMINFO and STRBUF. Format-specific
 * readers live in mulib-{elf,coff,pe,macho}-objdump.h. No disassembler. */
#ifndef MULIB_OBJDUMP_H
#define MULIB_OBJDUMP_H
#include "mulib-termstyle.h"
#ifdef __cplusplus
extern "C" {
#endif
enum mulib_objdump_flags {
  MULIB_OBJDUMP_HEADERS = 1, MULIB_OBJDUMP_SECTIONS = 2,
  MULIB_OBJDUMP_SYMBOLS = 4, MULIB_OBJDUMP_CONTENTS = 8,
  MULIB_OBJDUMP_ALL = 15
};
/** @brief NULL options select headers/sections/symbols, 256 content bytes per
 * section, 4096 records per table and plain output. Zero limits mean zero,
 * not unlimited. Record truncation fails; content truncation is marked. */
typedef struct mulib_objdump_options {
  unsigned flags;
  size_t max_bytes, max_records;
  const mulib_termstyle_t *style;
} mulib_objdump_options_t;
/** @brief Initialize documented defaults. */
void mulib_objdump_options_init (mulib_objdump_options_t *options);
/** @brief Escape non-ASCII/control bytes and backslash as \\xHH, atomically. */
int mulib_objdump_name (mulib_strbuf_t *out, const char *name, size_t length);
/** @brief Render one format/header line; uses optional bold terminfo style. */
int mulib_objdump_header (mulib_strbuf_t *out, const char *format, unsigned bits,
    uint32_t machine, uint32_t type, const mulib_termstyle_t *style);
/** @brief Render one numbered section record, escaping its bounded name. */
int mulib_objdump_section (mulib_strbuf_t *out, size_t index, const char *name, size_t length,
    uint64_t address, uint64_t offset, uint64_t size, uint64_t flags);
/** @brief Render one numbered symbol record; section may be negative. */
int mulib_objdump_symbol (mulib_strbuf_t *out, size_t index, const char *name, size_t length,
    uint64_t value, int64_t section);
/** @brief Hex/ASCII rows, 16 bytes per row; at most limit input bytes.
 * Mark truncation; reject offset overflow. Every renderer appends atomically. */
int mulib_objdump_hex (mulib_strbuf_t *out, const void *data, size_t length, uint64_t offset, size_t limit);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OBJDUMP) && !defined(MULIB_OBJDUMP_IMPLEMENTED)
#define MULIB_OBJDUMP_IMPLEMENTED
#include <inttypes.h>
#include <stdio.h>
void mulib_objdump_options_init (mulib_objdump_options_t *o)
{ if (o) { o->flags = 7; o->max_bytes = 256; o->max_records = 4096; o->style = NULL; } }
int mulib_objdump_name (mulib_strbuf_t *out, const char *name, size_t n)
{
  static const char digits[] = "0123456789abcdef";
  mulib_strbuf_t tmp = { 0 };
  size_t i;
  int ok = 0;
  if (!out || (!name && n)) return 0;
  for (i = 0; i < n; ++i) {
    unsigned char c = (unsigned char)name[i];
    if (c >= 32 && c < 127 && c != '\\') { if (!mulib_strbuf_putc (&tmp, (char)c)) goto done; }
    else {
      char escaped[4] = { '\\', 'x', digits[c >> 4], digits[c & 15] };
      if (!mulib_strbuf_append_n (&tmp, escaped, 4)) goto done;
    }
  }
  ok = mulib_strbuf_append_n (out, tmp.data, tmp.length);
done:
  mulib_strbuf_free (&tmp); return ok;
}
int mulib_objdump_header (mulib_strbuf_t *out, const char *format, unsigned bits,
    uint32_t machine, uint32_t type, const mulib_termstyle_t *style)
{
  mulib_strbuf_t tmp = { 0 }, title = { 0 };
  mulib_termstyle_spec_t bold = { MULIB_TERMSTYLE_BOLD, -1, -1 };
  char numbers[96];
  int n, ok = 0;
  if (!out || !format) return 0;
  if (!mulib_objdump_name (&title, format, strlen (format))) goto done;
  n = snprintf (numbers, sizeof (numbers), " bits=%u machine=0x%" PRIx32 " type=0x%" PRIx32 "\n", bits, machine, type);
  if (n < 0 || (size_t)n >= sizeof (numbers)) goto done;
  if (style ? !mulib_termstyle_text (style, &bold, title.data, title.length, &tmp)
            : !mulib_strbuf_append_n (&tmp, title.data, title.length)) goto done;
  ok = mulib_strbuf_append_n (&tmp, numbers, (size_t)n) && mulib_strbuf_append_n (out, tmp.data, tmp.length);
done:
  mulib_strbuf_free (&tmp); mulib_strbuf_free (&title); return ok;
}
int mulib_objdump_section (mulib_strbuf_t *out, size_t i, const char *name, size_t len,
    uint64_t address, uint64_t offset, uint64_t size, uint64_t flags)
{
  mulib_strbuf_t tmp = { 0 };
  char number[192];
  int n, ok = 0;
  if (!out) return 0;
  n = snprintf (number, sizeof (number), "section[%zu] ", i);
  if (n < 0 || (size_t)n >= sizeof (number) || !mulib_strbuf_append_n (&tmp, number, (size_t)n)
      || !mulib_objdump_name (&tmp, name, len)) goto done;
  n = snprintf (number, sizeof (number), " address=0x%" PRIx64 " offset=0x%" PRIx64 " size=0x%" PRIx64 " flags=0x%" PRIx64 "\n",
      address, offset, size, flags);
  ok = n >= 0 && (size_t)n < sizeof (number) && mulib_strbuf_append_n (&tmp, number, (size_t)n)
      && mulib_strbuf_append_n (out, tmp.data, tmp.length);
done:
  mulib_strbuf_free (&tmp); return ok;
}
int mulib_objdump_symbol (mulib_strbuf_t *out, size_t i, const char *name, size_t len, uint64_t value, int64_t section)
{
  mulib_strbuf_t tmp = { 0 };
  char number[128];
  int n, ok = 0;
  if (!out) return 0;
  n = snprintf (number, sizeof (number), "symbol[%zu] ", i);
  if (n < 0 || (size_t)n >= sizeof (number) || !mulib_strbuf_append_n (&tmp, number, (size_t)n)
      || !mulib_objdump_name (&tmp, name, len)) goto done;
  n = snprintf (number, sizeof (number), " value=0x%" PRIx64 " section=%" PRId64 "\n", value, section);
  ok = n >= 0 && (size_t)n < sizeof (number) && mulib_strbuf_append_n (&tmp, number, (size_t)n)
      && mulib_strbuf_append_n (out, tmp.data, tmp.length);
done:
  mulib_strbuf_free (&tmp); return ok;
}
int mulib_objdump_hex (mulib_strbuf_t *out, const void *data, size_t n, uint64_t off, size_t limit)
{
  const unsigned char *p = (const unsigned char *)data;
  mulib_strbuf_t tmp = { 0 };
  size_t count = n < limit ? n : limit, at = 0;
  int ok = 0;
  if (!out || (!p && n) || count > UINT64_MAX - off) return 0;
  while (at < count) {
    char row[96]; size_t i, width = count - at, used;
    int k;
    if (width > 16) width = 16;
    k = snprintf (row, sizeof (row), "%016" PRIx64 "  ", off + at);
    if (k != 18) goto done;
    used = (size_t)k;
    for (i = 0; i < 16; ++i) {
      k = i < width ? snprintf (row + used, sizeof (row) - used, "%02x ", (unsigned)p[at + i])
                    : snprintf (row + used, sizeof (row) - used, "   ");
      if (k != 3) goto done;
      used += 3;
    }
    row[used++] = '|';
    for (i = 0; i < width; ++i) { unsigned c = p[at + i]; row[used++] = c >= 32 && c < 127 ? (char)c : '.'; }
    row[used++] = '|'; row[used++] = '\n';
    if (!mulib_strbuf_append_n (&tmp, row, used)) goto done;
    at += width;
  }
  if (count < n && !mulib_strbuf_append (&tmp, "... truncated ...\n")) goto done;
  ok = mulib_strbuf_append_n (out, tmp.data, tmp.length);
done:
  mulib_strbuf_free (&tmp); return ok;
}
#endif
