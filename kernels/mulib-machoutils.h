/** @file mulib-machoutils.h
 * @brief Exact Mach-O section and defined non-debug symbol lookup.
 * Define MULIB_IMPL_MACHOUTILS, MACHO and ENDIAN. */
#ifndef MULIB_MACHOUTILS_H
#define MULIB_MACHOUTILS_H
#include "mulib-macho.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief First exact section; NULL segment matches any segment. Optional index. */
int mulib_machoutils_section (const mulib_macho_t *v, const char *segment, const char *name, size_t *index, mulib_macho_section_t *out);
/** @brief First exact defined N_SECT/N_ABS symbol; no underscore rewriting. */
int mulib_machoutils_symbol (const mulib_macho_t *v, const char *name, mulib_macho_symbol_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MACHOUTILS) && !defined(MULIB_MACHOUTILS_IMPLEMENTED)
#define MULIB_MACHOUTILS_IMPLEMENTED
#include <string.h>
int mulib_machoutils_section (const mulib_macho_t *v, const char *segment, const char *name, size_t *index, mulib_macho_section_t *out)
{
  size_t i;
  if (!v || !v->data || !name || !out) return MULIB_MACHO_EINVAL;
  for (i = 0; i < v->section_count; ++i) {
    mulib_macho_section_t s;
    int rc = mulib_macho_section (v, i, &s); if (rc) return rc;
    if (!strcmp (s.name, name) && (!segment || !strcmp (s.segment, segment))) {
      *out = s; if (index) *index = i; return 0;
    }
  }
  return MULIB_MACHO_ENOTFOUND;
}
int mulib_machoutils_symbol (const mulib_macho_t *v, const char *name, mulib_macho_symbol_t *out)
{
  size_t i, len;
  if (!v || !v->data || !name || !out) return MULIB_MACHO_EINVAL;
  len = strlen (name);
  for (i = 0; i < v->symbol_count; ++i) {
    mulib_macho_symbol_t sym;
    const char *s; size_t n;
    int rc = mulib_macho_symbol (v, i, &sym); if (rc) return rc;
    rc = mulib_macho_symbol_name (v, i, &s, &n); if (rc) return rc;
    if (!(sym.type & 0xe0u) && ((sym.type & 0x0eu) == 0x0eu || (sym.type & 0x0eu) == 2)
        && n == len && !memcmp (s, name, n)) { *out = sym; return 0; }
  }
  return MULIB_MACHO_ENOTFOUND;
}
#endif
