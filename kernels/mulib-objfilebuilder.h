/** @file mulib-objfilebuilder.h
 * @brief Owned section builder for the focused ELF64 object writer.
 * Define MULIB_IMPL_OBJFILEBUILDER and all OBJFILEWRITER dependencies. */
#ifndef MULIB_OBJFILEBUILDER_H
#define MULIB_OBJFILEBUILDER_H
#include "mulib-objfilewriter.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_objfilebuilder
  {
    mulib_objfilewriter_section_t *sections;
    size_t count;
  } mulib_objfilebuilder_t;
  /** @brief Zero init. Copy a unique section; 1 success, failure unchanged. */
  int mulib_objfilebuilder_add (mulib_objfilebuilder_t *b, const char *name,
                                const void *data, size_t size,
                                size_t alignment, uint64_t flags);
  /** @brief Append complete ELF64 object without consuming builder. */
  int mulib_objfilebuilder_finish (const mulib_objfilebuilder_t *b,
                                   uint16_t machine, mulib_buffer_t *out);
  /** @brief Release all section names/bytes and reset. */
  void mulib_objfilebuilder_free (mulib_objfilebuilder_t *b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OBJFILEBUILDER)                                        \
    && !defined(MULIB_OBJFILEBUILDER_IMPLEMENTED)
#define MULIB_OBJFILEBUILDER_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
int
mulib_objfilebuilder_add (mulib_objfilebuilder_t *b, const char *name,
                          const void *p, size_t n, size_t a, uint64_t flags)
{
  char *s;
  void *data;
  mulib_objfilewriter_section_t *next;
  size_t i, len;
  if (!b || !name || !*name || !strcmp (name, ".shstrtab") || (!p && n) || !a
      || (a & (a - 1)) || (flags & ~UINT64_C (7)) || b->count >= 65277)
    return 0;
  for (i = 0; i < b->count; ++i)
    if (!strcmp (b->sections[i].name, name))
      return 0;
  len = strlen (name);
  if (len == SIZE_MAX)
    return 0;
  s = (char *)malloc (len + 1);
  data = malloc (n ? n : 1);
  if (!s || !data)
    {
      free (s);
      free (data);
      return 0;
    }
  memcpy (s, name, len + 1);
  if (n)
    memcpy (data, p, n);
  next = (mulib_objfilewriter_section_t *)realloc (
      b->sections, (b->count + 1) * sizeof (*next));
  if (!next)
    {
      free (s);
      free (data);
      return 0;
    }
  b->sections = next;
  next += b->count++;
  next->name = s;
  next->data = data;
  next->size = n;
  next->alignment = a;
  next->flags = flags;
  return 1;
}
int
mulib_objfilebuilder_finish (const mulib_objfilebuilder_t *b, uint16_t machine,
                             mulib_buffer_t *out)
{
  return b && mulib_objfilewriter_write (out, machine, b->sections, b->count);
}
void
mulib_objfilebuilder_free (mulib_objfilebuilder_t *b)
{
  size_t i;
  if (!b)
    return;
  for (i = 0; i < b->count; ++i)
    {
      free ((void *)b->sections[i].name);
      free ((void *)b->sections[i].data);
    }
  free (b->sections);
  b->sections = NULL;
  b->count = 0;
}
#endif
