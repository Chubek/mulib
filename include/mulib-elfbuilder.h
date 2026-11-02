/** @file mulib-elfbuilder.h
 * @brief Owned ELF64 relocatable section builder reusing objfilebuilder.
 * Define MULIB_IMPL_ELFBUILDER and OBJFILEBUILDER dependencies.
 * No symbols, relocations or executable segments; see objfilewriter profile. */
#ifndef MULIB_ELFBUILDER_H
#define MULIB_ELFBUILDER_H
#include "mulib-objfilebuilder.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef mulib_objfilebuilder_t mulib_elfbuilder_t;
  int mulib_elfbuilder_add (mulib_elfbuilder_t *builder, const char *name,
                            const void *data, size_t size, size_t alignment,
                            uint64_t flags);
  int mulib_elfbuilder_finish (const mulib_elfbuilder_t *builder,
                               uint16_t machine, mulib_buffer_t *out);
  void mulib_elfbuilder_free (mulib_elfbuilder_t *builder);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ELFBUILDER) && !defined(MULIB_ELFBUILDER_IMPLEMENTED)
#define MULIB_ELFBUILDER_IMPLEMENTED
int
mulib_elfbuilder_add (mulib_elfbuilder_t *b, const char *name, const void *p,
                      size_t n, size_t a, uint64_t flags)
{
  return mulib_objfilebuilder_add (b, name, p, n, a, flags);
}
int
mulib_elfbuilder_finish (const mulib_elfbuilder_t *b, uint16_t machine,
                         mulib_buffer_t *out)
{
  return mulib_objfilebuilder_finish (b, machine, out);
}
void
mulib_elfbuilder_free (mulib_elfbuilder_t *b)
{
  mulib_objfilebuilder_free (b);
}
#endif
