/** @file mulib-objfile.h
 * @brief Borrowed ELF relocatable-object model; other object formats rejected.
 * Define MULIB_IMPL_OBJFILE plus ELFUTILS, ELF and ENDIAN. */
#ifndef MULIB_OBJFILE_H
#define MULIB_OBJFILE_H
#include "mulib-binfile.h"
#include "mulib-elfutils.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_objfile
  {
    mulib_elf_t elf;
  } mulib_objfile_t;
  /** @brief Open ET_REL ELF32/64; 0 success, ELF error otherwise. No ownership
   * transfer. */
  int mulib_objfile_open (mulib_objfile_t *out, const void *data, size_t size);
  /** @brief Borrow named file-backed section; NOBITS returns ETYPE. */
  int mulib_objfile_section (const mulib_objfile_t *object, const char *name,
                             mulib_binfile_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OBJFILE) && !defined(MULIB_OBJFILE_IMPLEMENTED)
#define MULIB_OBJFILE_IMPLEMENTED
int
mulib_objfile_open (mulib_objfile_t *out, const void *p, size_t n)
{
  mulib_objfile_t v;
  int rc;
  if (!out)
    return MULIB_ELF_EINVAL;
  rc = mulib_elf_open (&v.elf, p, n);
  if (rc)
    return rc;
  if (v.elf.type != 1)
    return MULIB_ELF_ETYPE;
  *out = v;
  return 0;
}
int
mulib_objfile_section (const mulib_objfile_t *v, const char *name,
                       mulib_binfile_t *out)
{
  mulib_elf_section_t s;
  const unsigned char *p;
  int rc;
  if (!v || !out)
    return MULIB_ELF_EINVAL;
  rc = mulib_elfutils_section (&v->elf, name, NULL, &s);
  if (rc)
    return rc;
  if (s.type == 8)
    return MULIB_ELF_ETYPE;
  rc = mulib_elf_bytes (&v->elf, s.offset, s.size, &p);
  if (rc)
    return rc;
  out->data = p;
  out->size = (size_t)s.size;
  return 0;
}
#endif
