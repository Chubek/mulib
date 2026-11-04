/** @file mulib-pe-objdump.h
 * @brief PE header, section, directory and COFF-symbol dumps over shared objdump.
 * Define MULIB_IMPL_PE_OBJDUMP plus PE/COFF_OBJDUMP dependencies. */
#ifndef MULIB_PE_OBJDUMP_H
#define MULIB_PE_OBJDUMP_H
#include "mulib-pe.h"
#include "mulib-coff-objdump.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Atomic append, 1 success/0 malformed, limit or OOM. No disassembly. */
int mulib_pe_objdump (const void *data, size_t size, const mulib_objdump_options_t *options, mulib_strbuf_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PE_OBJDUMP) && !defined(MULIB_PE_OBJDUMP_IMPLEMENTED)
#define MULIB_PE_OBJDUMP_IMPLEMENTED
int mulib_pe_objdump (const void *data, size_t n, const mulib_objdump_options_t *options, mulib_strbuf_t *out)
{
  mulib_pe_t v;
  mulib_objdump_options_t o;
  mulib_strbuf_t tmp = { 0 };
  size_t i;
  int ok = 0;
  if (!out || mulib_pe_open (&v, data, n)) return 0;
  mulib_objdump_options_init (&o); if (options) o = *options;
  if (o.flags & ~15u) return 0;
  if (o.flags & 1) {
    if (!mulib_objdump_header (&tmp, "PE", v.bits, v.coff.machine, v.coff.characteristics, o.style)
        || !mulib_objdump_section (&tmp, 0, "image", 5, v.image_base, v.entry, v.image_size, v.subsystem)
        || v.directory_count > o.max_records) goto done;
    for (i = 0; i < v.directory_count; ++i) {
      mulib_pe_directory_t d;
      if (mulib_pe_directory (&v, i, &d)) goto done;
      if (d.size && !mulib_objdump_section (&tmp, i, "directory", 9, d.address, i == 4 ? d.address : 0, d.size, 0)) goto done;
    }
  }
  o.flags &= ~1u;
  if (!mulib_coff_objdump_view (&v.coff, &o, &tmp)) goto done;
  ok = mulib_strbuf_append_n (out, tmp.data, tmp.length);
done:
  mulib_strbuf_free (&tmp); return ok;
}
#endif
