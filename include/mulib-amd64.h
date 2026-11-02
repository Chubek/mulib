/** @file mulib-amd64.h
 * @brief Owned AMD64 metadata assembler convenience context.
 * Define MULIB_IMPL_AMD64 and all ASSEMBLER dependencies. Encodings and the
 * supported register/immediate/branch subset come from dsymspec, not a second
 * instruction table. Zero initialize; free before reinitializing. */
#ifndef MULIB_AMD64_H
#define MULIB_AMD64_H
#include "mulib-assembler.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_amd64
  {
    mulib_dsymspec_t *specification;
  } mulib_amd64_t;
  int mulib_amd64_init (mulib_amd64_t *context);
  void mulib_amd64_free (mulib_amd64_t *context);
  int mulib_amd64_emit (const mulib_amd64_t *context, mulib_emitter_t *image,
                        mulib_linker_t *linker, const char *mnemonic,
                        const mulib_assembler_operand_t *operands, size_t count,
                        mulib_assembler_error_t *error);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_AMD64) && !defined(MULIB_AMD64_IMPLEMENTED)
#define MULIB_AMD64_IMPLEMENTED
int
mulib_amd64_init (mulib_amd64_t *c)
{
  if (!c || c->specification)
    return 0;
  c->specification = mulib_dsymspec_load_builtin ("amd64", NULL);
  return c->specification != NULL;
}
void
mulib_amd64_free (mulib_amd64_t *c)
{
  if (c)
    {
      mulib_dsymspec_destroy (c->specification);
      c->specification = NULL;
    }
}
int
mulib_amd64_emit (const mulib_amd64_t *c, mulib_emitter_t *image,
                  mulib_linker_t *linker, const char *name,
                  const mulib_assembler_operand_t *operands, size_t n,
                  mulib_assembler_error_t *error)
{
  if (!c || !c->specification)
    return MULIB_ASSEMBLER_EINVAL;
  return mulib_assembler_emit (c->specification, image, linker, name, operands,
                               n, error);
}
#endif
