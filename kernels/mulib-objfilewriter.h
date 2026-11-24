/** @file mulib-objfilewriter.h
 * @brief Deterministic ELF64 little-endian ET_REL writer for PROGBITS
 * sections. Define MULIB_IMPL_OBJFILEWRITER plus BINFILEWRITER, BYCUTILS and
 * BUFFER. No program headers, symbols, relocations, extended section counts or
 * execution. */
#ifndef MULIB_OBJFILEWRITER_H
#define MULIB_OBJFILEWRITER_H
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Borrowed section input. Name nonempty/unique, not .shstrtab.
   * alignment is power-of-two; flags restricted to WRITE/ALLOC/EXECINSTR (bits
   * 0..2). */
  typedef struct mulib_objfilewriter_section
  {
    const char *name;
    const void *data;
    size_t size, alignment;
    uint64_t flags;
  } mulib_objfilewriter_section_t;
  /** @brief Append a complete object atomically; machine is ELF e_machine
   * (e.g. 62). 1 success, 0 invalid/overflow/OOM. Input may alias output
   * (built in temporary). */
  int mulib_objfilewriter_write (mulib_buffer_t *out, uint16_t machine,
                                 const mulib_objfilewriter_section_t *sections,
                                 size_t count);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OBJFILEWRITER)                                         \
    && !defined(MULIB_OBJFILEWRITER_IMPLEMENTED)
#define MULIB_OBJFILEWRITER_IMPLEMENTED
#include "mulib-binfilewriter.h"
#include "mulib-bycutils.h"
#include <string.h>
int
mulib_objfilewriter_write (mulib_buffer_t *out, uint16_t machine,
                           const mulib_objfilewriter_section_t *s, size_t n)
{
  mulib_buffer_t b = { 0 }, names = { 0 }, headers = { 0 };
  unsigned char h[64] = { 0 };
  size_t i, j, table;
  int ok = 0;
  if (!out || !machine || (!s && n) || n > 65277)
    return 0;
  memcpy (h, "\177ELF\2\1\1", 7);
  if (!mulib_buffer_append (&b, h, 64)
      || !mulib_buffer_append (&headers, h + 8, 56)
      || !mulib_buffer_append (&headers, h + 8, 8)
      || !mulib_buffer_append (&names, "", 1))
    goto done;
  for (i = 0; i < n; ++i)
    {
      size_t nameoff = names.length;
      if (!s[i].name || !*s[i].name || !strcmp (s[i].name, ".shstrtab")
          || (!s[i].data && s[i].size) || !s[i].alignment
          || (s[i].alignment & (s[i].alignment - 1))
          || (s[i].flags & ~UINT64_C (7)))
        goto done;
      for (j = 0; j < i; ++j)
        if (!strcmp (s[i].name, s[j].name))
          goto done;
      if (nameoff > UINT32_MAX
          || !mulib_buffer_append (&names, s[i].name, strlen (s[i].name) + 1)
          || !mulib_binfilewriter_align (&b, s[i].alignment))
        goto done;
      memset (h, 0, 64);
      mulib_bycutils_store (h, 64, 0, 4, nameoff);
      mulib_bycutils_store (h, 64, 4, 4, 1);
      mulib_bycutils_store (h, 64, 8, 8, s[i].flags);
      mulib_bycutils_store (h, 64, 24, 8, b.length);
      mulib_bycutils_store (h, 64, 32, 8, s[i].size);
      mulib_bycutils_store (h, 64, 48, 8, s[i].alignment);
      if (!mulib_buffer_append (&headers, h, 64)
          || !mulib_buffer_append (&b, s[i].data, s[i].size))
        goto done;
    }
  if (names.length > UINT32_MAX)
    goto done;
  memset (h, 0, 64);
  mulib_bycutils_store (h, 64, 0, 4, names.length);
  mulib_bycutils_store (h, 64, 4, 4, 3);
  mulib_bycutils_store (h, 64, 24, 8, b.length);
  mulib_bycutils_store (h, 64, 48, 8, 1);
  if (!mulib_buffer_append (&names, ".shstrtab", 10))
    goto done;
  mulib_bycutils_store (h, 64, 32, 8, names.length);
  if (!mulib_buffer_append (&headers, h, 64)
      || !mulib_buffer_append (&b, names.data, names.length)
      || !mulib_binfilewriter_align (&b, 8))
    goto done;
  table = b.length;
  if (!mulib_buffer_append (&b, headers.data, headers.length))
    goto done;
  mulib_binfilewriter_patch (&b, 16, 2, 1);
  mulib_binfilewriter_patch (&b, 18, 2, machine);
  mulib_binfilewriter_patch (&b, 20, 4, 1);
  mulib_binfilewriter_patch (&b, 40, 8, table);
  mulib_binfilewriter_patch (&b, 52, 2, 64);
  mulib_binfilewriter_patch (&b, 58, 2, 64);
  mulib_binfilewriter_patch (&b, 60, 2, n + 2);
  mulib_binfilewriter_patch (&b, 62, 2, n + 1);
  ok = mulib_buffer_append (out, b.data, b.length);
done:
  mulib_buffer_free (&b);
  mulib_buffer_free (&names);
  mulib_buffer_free (&headers);
  return ok;
}
#endif
