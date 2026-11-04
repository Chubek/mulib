/** @file mulib-coff-objdump.h
 * @brief COFF metadata, symbols and section hex dumps using shared objdump tools.
 * Define MULIB_IMPL_COFF_OBJDUMP plus COFF/OBJDUMP dependencies. */
#ifndef MULIB_COFF_OBJDUMP_H
#define MULIB_COFF_OBJDUMP_H
#include "mulib-coff.h"
#include "mulib-objdump.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Append a complete dump atomically; 1 success, 0 malformed/limit/OOM.
 * Auxiliary symbols and zero-fill bytes are not printed as real records/data. */
int mulib_coff_objdump (const void *data, size_t size, const mulib_objdump_options_t *options, mulib_strbuf_t *out);
/** @brief Same renderer for an already opened view, also used by PE. */
int mulib_coff_objdump_view (const mulib_coff_t *v, const mulib_objdump_options_t *options, mulib_strbuf_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_COFF_OBJDUMP) && !defined(MULIB_COFF_OBJDUMP_IMPLEMENTED)
#define MULIB_COFF_OBJDUMP_IMPLEMENTED
int mulib_coff_objdump_view (const mulib_coff_t *v, const mulib_objdump_options_t *options, mulib_strbuf_t *out)
{
  mulib_objdump_options_t defaults;
  mulib_strbuf_t tmp = { 0 };
  size_t i, records = 0;
  int ok = 0;
  mulib_objdump_options_init (&defaults); if (!options) options = &defaults;
  if (!v || !v->data || !out || (options->flags & ~15u)) return 0;
  if ((options->flags & 1) && !mulib_objdump_header (&tmp, "COFF", 0, v->machine, v->characteristics, options->style)) goto done;
  if ((options->flags & 10) && v->section_count > options->max_records) goto done;
  for (i = 0; (options->flags & 10) && i < v->section_count; ++i) {
    mulib_coff_section_t s;
    const char *name; size_t len;
    const unsigned char *bytes;
    if (mulib_coff_section (v, i, &s) || mulib_coff_section_name (v, i, &name, &len)) goto done;
    if (!mulib_objdump_section (&tmp, i, name, len, s.address, s.offset, s.size, s.flags)) goto done;
    if ((options->flags & 8) && s.offset && s.size) {
      if (mulib_coff_bytes (v, s.offset, s.size, &bytes)
          || !mulib_objdump_hex (&tmp, bytes, s.size, s.offset, options->max_bytes)) goto done;
    }
  }
  for (i = 0; (options->flags & 4) && i < v->symbol_count;) {
    mulib_coff_symbol_t s;
    const char *name; size_t len;
    if (records++ >= options->max_records || mulib_coff_symbol (v, i, &s)
        || mulib_coff_symbol_name (v, i, &name, &len)
        || !mulib_objdump_symbol (&tmp, i, name, len, s.value, s.section_index)) goto done;
    i += 1u + s.auxiliary_count;
  }
  ok = mulib_strbuf_append_n (out, tmp.data, tmp.length);
done:
  mulib_strbuf_free (&tmp); return ok;
}
int mulib_coff_objdump (const void *data, size_t n, const mulib_objdump_options_t *options, mulib_strbuf_t *out)
{
  mulib_coff_t v;
  return !mulib_coff_open (&v, data, n) && mulib_coff_objdump_view (&v, options, out);
}
#endif
