/** @file mulib-objfilesymloc.h
 * @brief Resolve a defined ELF symbol's file-backed bytes, not its runtime
 * address. Define MULIB_IMPL_OBJFILESYMLOC plus ELFUTILS, ELF and ENDIAN. */
#ifndef MULIB_OBJFILESYMLOC_H
#define MULIB_OBJFILESYMLOC_H
#include "mulib-binfile.h"
#include "mulib-elfutils.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief ET_REL st_value is section-relative; EXEC/DYN values are virtual.
   * Reject ABS/COMMON/undefined/NOBITS and section-crossing symbols.
   * 0 success, ELF error otherwise. Output borrows input. */
  int mulib_objfilesymloc_find (const mulib_elf_t *elf, const char *name,
                                mulib_binfile_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OBJFILESYMLOC)                                         \
    && !defined(MULIB_OBJFILESYMLOC_IMPLEMENTED)
#define MULIB_OBJFILESYMLOC_IMPLEMENTED
int
mulib_objfilesymloc_find (const mulib_elf_t *v, const char *name,
                          mulib_binfile_t *out)
{
  mulib_elf_symbol_t sym;
  mulib_elf_section_t s;
  uint64_t off;
  const unsigned char *p;
  int rc;
  if (!v || !out)
    return MULIB_ELF_EINVAL;
  rc = mulib_elfutils_symbol (v, name, &sym);
  if (rc)
    return rc;
  if (!sym.section_index || sym.section_index >= v->shnum)
    return MULIB_ELF_ETYPE;
  rc = mulib_elf_section (v, sym.section_index, &s);
  if (rc)
    return rc;
  if (s.type == 8 || (v->type != 1 && v->type != 2 && v->type != 3))
    return MULIB_ELF_ETYPE;
  off = sym.value;
  if (v->type != 1)
    {
      if (off < s.address)
        return MULIB_ELF_EBOUNDS;
      off -= s.address;
    }
  if (off > s.size || sym.size > s.size - off)
    return MULIB_ELF_EBOUNDS;
  rc = mulib_elf_bytes (v, s.offset + off, sym.size, &p);
  if (rc)
    return rc;
  out->data = p;
  out->size = (size_t)sym.size;
  return 0;
}
#endif
