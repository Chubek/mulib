/** @file mulib-macho-objdump.h
 * @brief Thin Mach-O command, section, symbol and hex dumps using shared objdump.
 * Define MULIB_IMPL_MACHO_OBJDUMP plus MACHO/OBJDUMP dependencies. */
#ifndef MULIB_MACHO_OBJDUMP_H
#define MULIB_MACHO_OBJDUMP_H
#include "mulib-macho.h"
#include "mulib-objdump.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Atomic append; 1 success, 0 malformed, limit, missing style or OOM. */
int mulib_macho_objdump (const void *data, size_t size, const mulib_objdump_options_t *options, mulib_strbuf_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MACHO_OBJDUMP) && !defined(MULIB_MACHO_OBJDUMP_IMPLEMENTED)
#define MULIB_MACHO_OBJDUMP_IMPLEMENTED
int mulib_macho_objdump (const void *data, size_t n, const mulib_objdump_options_t *options, mulib_strbuf_t *out)
{
  mulib_macho_t v;
  mulib_objdump_options_t defaults;
  mulib_strbuf_t tmp = { 0 };
  size_t i;
  int ok = 0;
  mulib_objdump_options_init (&defaults); if (!options) options = &defaults;
  if (!out || (options->flags & ~15u) || mulib_macho_open (&v, data, n)) return 0;
  if (options->flags & 1) {
    if (v.command_count > options->max_records
        || !mulib_objdump_header (&tmp, "Mach-O", v.bits, v.cpu, v.type, options->style)) goto done;
    for (i = 0; i < v.command_count; ++i) {
      mulib_macho_command_t c;
      if (mulib_macho_command (&v, i, &c)
          || !mulib_objdump_section (&tmp, i, "load-command", 12, 0, (uint64_t)(c.data - v.data), c.size, c.type)) goto done;
    }
  }
  if ((options->flags & 10) && v.section_count > options->max_records) goto done;
  for (i = 0; (options->flags & 10) && i < v.section_count; ++i) {
    mulib_macho_section_t s;
    const unsigned char *bytes;
    unsigned type;
    if (mulib_macho_section (&v, i, &s)
        || !mulib_objdump_section (&tmp, i, s.name, strlen (s.name), s.address, s.offset, s.size, s.flags)) goto done;
    type = s.flags & 255u;
    if ((options->flags & 8) && type != 1 && type != 0xc && type != 0x12) {
      if (mulib_macho_bytes (&v, s.offset, s.size, &bytes)
          || !mulib_objdump_hex (&tmp, bytes, (size_t)s.size, s.offset, options->max_bytes)) goto done;
    }
  }
  if ((options->flags & 4) && v.symbol_count > options->max_records) goto done;
  for (i = 0; (options->flags & 4) && i < v.symbol_count; ++i) {
    mulib_macho_symbol_t s;
    const char *name; size_t len;
    if (mulib_macho_symbol (&v, i, &s) || mulib_macho_symbol_name (&v, i, &name, &len)
        || !mulib_objdump_symbol (&tmp, i, name, len, s.value, s.section_index)) goto done;
  }
  ok = mulib_strbuf_append_n (out, tmp.data, tmp.length);
done:
  mulib_strbuf_free (&tmp); return ok;
}
#endif
