/** @file mulib-coffutils.h
 * @brief Exact section and defined-primary-symbol lookup over checked COFF.
 * Define MULIB_IMPL_COFFUTILS, COFF and ENDIAN. */
#ifndef MULIB_COFFUTILS_H
#define MULIB_COFFUTILS_H
#include "mulib-coff.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief First exact section; optional index, required out, atomic on error. */
int mulib_coffutils_section (const mulib_coff_t *v, const char *name, size_t *index, mulib_coff_section_t *out);
/** @brief First named defined symbol (including absolute); skips auxiliaries. */
int mulib_coffutils_symbol (const mulib_coff_t *v, const char *name, mulib_coff_symbol_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_COFFUTILS) && !defined(MULIB_COFFUTILS_IMPLEMENTED)
#define MULIB_COFFUTILS_IMPLEMENTED
#include <string.h>
int mulib_coffutils_section (const mulib_coff_t *v, const char *name, size_t *index, mulib_coff_section_t *out)
{
  size_t i, len;
  if (!v || !v->data || !name || !out) return MULIB_COFF_EINVAL;
  len = strlen (name);
  for (i = 0; i < v->section_count; ++i) {
    const char *s; size_t n;
    int rc = mulib_coff_section_name (v, i, &s, &n); if (rc) return rc;
    if (n == len && !memcmp (s, name, n)) {
      rc = mulib_coff_section (v, i, out);
      if (!rc && index) *index = i;
      return rc;
    }
  }
  return MULIB_COFF_ENOTFOUND;
}
int mulib_coffutils_symbol (const mulib_coff_t *v, const char *name, mulib_coff_symbol_t *out)
{
  size_t i, len;
  if (!v || !v->data || !name || !out) return MULIB_COFF_EINVAL;
  len = strlen (name);
  for (i = 0; i < v->symbol_count;) {
    mulib_coff_symbol_t sym;
    const char *s; size_t n;
    int rc = mulib_coff_symbol (v, i, &sym); if (rc) return rc;
    rc = mulib_coff_symbol_name (v, i, &s, &n); if (rc) return rc;
    if ((sym.section_index > 0 || sym.section_index == -1) && n == len && !memcmp (s, name, n)) {
      *out = sym; return 0;
    }
    i += 1u + sym.auxiliary_count;
  }
  return MULIB_COFF_ENOTFOUND;
}
#endif
