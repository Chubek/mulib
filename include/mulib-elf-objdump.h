/** @file mulib-elf-objdump.h
 * @brief ELF32/64 program, section, symbol and hex dumps using shared objdump.
 * Define MULIB_IMPL_ELF_OBJDUMP plus ELF/OBJDUMP dependencies. */
#ifndef MULIB_ELF_OBJDUMP_H
#define MULIB_ELF_OBJDUMP_H
#include "mulib-elf.h"
#include "mulib-objdump.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Atomic append; 1 success/0 malformed, limit, style or allocation error. */
int mulib_elf_objdump (const void *data, size_t size, const mulib_objdump_options_t *options, mulib_strbuf_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ELF_OBJDUMP) && !defined(MULIB_ELF_OBJDUMP_IMPLEMENTED)
#define MULIB_ELF_OBJDUMP_IMPLEMENTED
int mulib_elf_objdump (const void *data, size_t n, const mulib_objdump_options_t *options, mulib_strbuf_t *out)
{
  mulib_elf_t v;
  mulib_objdump_options_t defaults;
  mulib_strbuf_t tmp = { 0 };
  size_t i, records = 0;
  int ok = 0;
  mulib_objdump_options_init (&defaults); if (!options) options = &defaults;
  if (!out || (options->flags & ~15u) || mulib_elf_open (&v, data, n)) return 0;
  if (options->flags & 1) {
    if (v.phnum > options->max_records
        || !mulib_objdump_header (&tmp, "ELF", v.bits, v.machine, v.type, options->style)) goto done;
    for (i = 0; i < v.phnum; ++i) {
      mulib_elf_program_t p;
      if (mulib_elf_program (&v, i, &p)
          || !mulib_objdump_section (&tmp, i, "program", 7, p.vaddr, p.offset, p.filesz, p.flags)) goto done;
    }
  }
  if ((options->flags & 14) && v.shnum > options->max_records) goto done;
  for (i = 0; (options->flags & 14) && i < v.shnum; ++i) {
    mulib_elf_section_t s;
    if (mulib_elf_section (&v, i, &s)) goto done;
    if (options->flags & 10) {
      const char *name = ""; size_t len = 0;
      int rc = mulib_elf_section_name (&v, i, &name, &len);
      if ((rc && rc != MULIB_ELF_ENOTFOUND)
          || !mulib_objdump_section (&tmp, i, name, len, s.address, s.offset, s.size, s.flags)) goto done;
      if ((options->flags & 8) && s.type != 0 && s.type != 8) {
        const unsigned char *bytes;
        if (mulib_elf_bytes (&v, s.offset, s.size, &bytes)
            || !mulib_objdump_hex (&tmp, bytes, (size_t)s.size, s.offset, options->max_bytes)) goto done;
      }
    }
    if ((options->flags & 4) && (s.type == 2 || s.type == 11)) {
      size_t j, count;
      if (mulib_elf_symbol_count (&v, i, &count) || count > options->max_records - records) goto done;
      records += count;
      for (j = 0; j < count; ++j) {
        mulib_elf_symbol_t sym;
        const char *name; size_t len;
        if (mulib_elf_symbol (&v, i, j, &sym) || mulib_elf_symbol_name (&v, i, j, &name, &len)
            || !mulib_objdump_symbol (&tmp, j, name, len, sym.value, sym.section_index)) goto done;
      }
    }
  }
  ok = mulib_strbuf_append_n (out, tmp.data, tmp.length);
done:
  mulib_strbuf_free (&tmp); return ok;
}
#endif
