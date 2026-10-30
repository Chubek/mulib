/** @file mulib-elfutils.h
 * @brief Named ELF section/symbol lookup over the checked ELF reader.
 * Define MULIB_IMPL_ELFUTILS plus ELF and ENDIAN. */
#ifndef MULIB_ELFUTILS_H
#define MULIB_ELFUTILS_H
#include "mulib-elf.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief First exact section name; 0 success, ENOTFOUND or ELF error. */
  int mulib_elfutils_section (const mulib_elf_t *elf, const char *name,
                              size_t *index, mulib_elf_section_t *out);
  /** @brief First defined named symbol in SYMTAB/DYNSYM, including local
   * symbols. 0 success, ENOTFOUND or structural error; output unchanged on
   * failure. */
  int mulib_elfutils_symbol (const mulib_elf_t *elf, const char *name,
                             mulib_elf_symbol_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ELFUTILS) && !defined(MULIB_ELFUTILS_IMPLEMENTED)
#define MULIB_ELFUTILS_IMPLEMENTED
#include <string.h>
int
mulib_elfutils_section (const mulib_elf_t *v, const char *name, size_t *index,
                        mulib_elf_section_t *out)
{
  size_t i, n, len;
  const char *s;
  int rc;
  if (!v || !name || !out)
    return MULIB_ELF_EINVAL;
  len = strlen (name);
  for (i = 1; i < v->shnum; ++i)
    {
      rc = mulib_elf_section_name (v, i, &s, &n);
      if (rc)
        return rc;
      if (n == len && !memcmp (s, name, n))
        {
          rc = mulib_elf_section (v, i, out);
          if (!rc && index)
            *index = i;
          return rc;
        }
    }
  return MULIB_ELF_ENOTFOUND;
}
int
mulib_elfutils_symbol (const mulib_elf_t *v, const char *name,
                       mulib_elf_symbol_t *out)
{
  size_t i, j, count, n, len;
  const char *s;
  mulib_elf_section_t sec;
  mulib_elf_symbol_t sym;
  int rc;
  if (!v || !name || !out)
    return MULIB_ELF_EINVAL;
  len = strlen (name);
  for (i = 1; i < v->shnum; ++i)
    {
      rc = mulib_elf_section (v, i, &sec);
      if (rc)
        return rc;
      if (sec.type != 2 && sec.type != 11)
        continue;
      rc = mulib_elf_symbol_count (v, i, &count);
      if (rc)
        return rc;
      for (j = 0; j < count; ++j)
        {
          rc = mulib_elf_symbol_name (v, i, j, &s, &n);
          if (rc)
            return rc;
          if (n == len && !memcmp (s, name, n))
            {
              rc = mulib_elf_symbol (v, i, j, &sym);
              if (rc)
                return rc;
              if (sym.section_index)
                {
                  *out = sym;
                  return 0;
                }
            }
        }
    }
  return MULIB_ELF_ENOTFOUND;
}
#endif
