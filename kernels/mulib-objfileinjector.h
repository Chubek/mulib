/** @file mulib-objfileinjector.h
 * @brief In-place, size-preserving replacement of file-backed ELF sections.
 * Define MULIB_IMPL_OBJFILEINJECTOR plus ELFUTILS, ELF, ENDIAN. No process
 * injection. */
#ifndef MULIB_OBJFILEINJECTOR_H
#define MULIB_OBJFILEINJECTOR_H
#include "mulib-elfutils.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Validate ELF and replace an entire named section only when lengths
   * match. Reject NULL/NOBITS/metadata sections (only PROGBITS type 1),
   * header overlap and overlap with any other file-backed section.
   * 0 success, ELF error otherwise. memmove permits overlapping replacement.
   */
  int mulib_objfileinjector_replace (void *object, size_t size,
                                     const char *section, const void *data,
                                     size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OBJFILEINJECTOR)                                       \
    && !defined(MULIB_OBJFILEINJECTOR_IMPLEMENTED)
#define MULIB_OBJFILEINJECTOR_IMPLEMENTED
#include <string.h>
int
mulib_objfileinjector_replace (void *p, size_t n, const char *name,
                               const void *data, size_t len)
{
  mulib_elf_t v;
  mulib_elf_section_t s, other;
  size_t index, i;
  uint64_t header_size;
  int rc;
  if (!p || (!data && len))
    return MULIB_ELF_EINVAL;
  rc = mulib_elf_open (&v, p, n);
  if (rc)
    return rc;
  rc = mulib_elfutils_section (&v, name, &index, &s);
  if (rc)
    return rc;
  if (s.type != 1)
    return MULIB_ELF_ETYPE;
  if (s.size != len)
    return MULIB_ELF_EBOUNDS;
  if (!len)
    return MULIB_ELF_OK;
  /* e_ehsize may include an extended header beyond the standard prefix. */
  i = v.bits == 32 ? 40 : 52;
  header_size = v.little_endian
                    ? (uint64_t)v.data[i] | (uint64_t)v.data[i + 1] << 8
                    : (uint64_t)v.data[i] << 8 | (uint64_t)v.data[i + 1];
  if (s.offset < header_size
      || (v.shnum && v.shoff < s.offset + s.size
          && s.offset < v.shoff + v.shnum * v.shentsize)
      || (v.phnum && v.phoff < s.offset + s.size
          && s.offset < v.phoff + v.phnum * v.phentsize))
    return MULIB_ELF_EFORMAT;
  for (i = 1; i < v.shnum; ++i)
    {
      if (i == index)
        continue;
      rc = mulib_elf_section (&v, i, &other);
      if (rc)
        return rc;
      if (other.type != 0 && other.type != 8 && other.size
          && s.offset < other.offset + other.size
          && other.offset < s.offset + s.size)
        return MULIB_ELF_EFORMAT;
    }
  if (len)
    memmove ((unsigned char *)p + (size_t)s.offset, data, len);
  return 0;
}
#endif
