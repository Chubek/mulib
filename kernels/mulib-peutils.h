/** @file mulib-peutils.h
 * @brief PE section and COFF debug-symbol lookup; not export resolution.
 * Define MULIB_IMPL_PEUTILS, PE, COFFUTILS, COFF and ENDIAN. */
#ifndef MULIB_PEUTILS_H
#define MULIB_PEUTILS_H
#include "mulib-pe.h"
#include "mulib-coffutils.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief First exact section, optional index; COFF result codes. */
int mulib_peutils_section (const mulib_pe_t *v, const char *name, size_t *index, mulib_pe_section_t *out);
/** @brief First defined COFF symbol; stripped images return ENOTFOUND. */
int mulib_peutils_symbol (const mulib_pe_t *v, const char *name, mulib_coff_symbol_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PEUTILS) && !defined(MULIB_PEUTILS_IMPLEMENTED)
#define MULIB_PEUTILS_IMPLEMENTED
int mulib_peutils_section (const mulib_pe_t *v, const char *name, size_t *i, mulib_pe_section_t *out)
{ return v ? mulib_coffutils_section (&v->coff, name, i, out) : MULIB_COFF_EINVAL; }
int mulib_peutils_symbol (const mulib_pe_t *v, const char *name, mulib_coff_symbol_t *out)
{ return v ? mulib_coffutils_symbol (&v->coff, name, out) : MULIB_COFF_EINVAL; }
#endif
