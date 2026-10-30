/** @file mulib-objfileleader.h
 * @brief ELF PT_LOAD copy/zero-fill helper (historical filename: leader).
 * Define MULIB_IMPL_OBJFILELEADER plus ELF/ENDIAN. No mapping, relocation or
 * execution. */
#ifndef MULIB_OBJFILELEADER_H
#define MULIB_OBJFILELEADER_H
#include "mulib-elf.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Copy EXEC/DYN load segments into caller memory representing
   * [base,base+size). Prevalidate all bounds and reject overlapping segments.
   * Input must not alias destination. Gaps untouched, BSS zeroed. 0 success,
   * ELF error otherwise. Permissions, interpreter, dynamic linking and
   * entry-point execution are caller-owned. */
  int mulib_objfileleader_load (const mulib_elf_t *elf, uint64_t base,
                                void *memory, size_t size);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OBJFILELEADER)                                         \
    && !defined(MULIB_OBJFILELEADER_IMPLEMENTED)
#define MULIB_OBJFILELEADER_IMPLEMENTED
#include <string.h>
int
mulib_objfileleader_load (const mulib_elf_t *v, uint64_t base, void *mem,
                          size_t n)
{
  size_t i, j;
  mulib_elf_program_t p, q;
  int rc;
  if (!v || (!mem && n))
    return MULIB_ELF_EINVAL;
  if (v->type != 2 && v->type != 3)
    return MULIB_ELF_ETYPE;
  for (i = 0; i < v->phnum; ++i)
    {
      rc = mulib_elf_program (v, i, &p);
      if (rc)
        return rc;
      if (p.type != 1 || !p.memsz)
        continue;
      if (p.vaddr < base || p.vaddr - base > n
          || p.memsz > n - (size_t)(p.vaddr - base))
        return MULIB_ELF_EBOUNDS;
      for (j = 0; j < i; ++j)
        {
          rc = mulib_elf_program (v, j, &q);
          if (rc)
            return rc;
          if (q.type == 1 && q.memsz
              && (p.vaddr >= q.vaddr ? p.vaddr - q.vaddr < q.memsz
                                     : q.vaddr - p.vaddr < p.memsz))
            return MULIB_ELF_EFORMAT;
        }
    }
  for (i = 0; i < v->phnum; ++i)
    {
      rc = mulib_elf_program (v, i, &p);
      if (rc)
        return rc;
      if (p.type == 1 && p.memsz)
        {
          unsigned char *dst = (unsigned char *)mem + (size_t)(p.vaddr - base);
          if (p.filesz)
            memcpy (dst, v->data + (size_t)p.offset, (size_t)p.filesz);
          memset (dst + (size_t)p.filesz, 0, (size_t)(p.memsz - p.filesz));
        }
    }
  return 0;
}
#endif
