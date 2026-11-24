/** @file mulib-amd64.h
 * @brief Owned AMD64 assembler and enriched MPU architecture context.
 * Define MULIB_IMPL_AMD64 and all ASSEMBLER and MPU dependencies. The legacy
 * typed assembler uses dsymspec; concrete-form codecs and queries use ArchML.
 * Zero initialize; free before reinitializing. Never copy ownership. */
#ifndef MULIB_AMD64_H
#define MULIB_AMD64_H
#include "mulib-assembler.h"
#include "mulib-mpu.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_amd64
  {
    mulib_dsymspec_t *specification;
    mulib_mpu_t mpu;
  } mulib_amd64_t;
  /** @brief Initialize both metadata owners atomically. Return 1/0. */
  int mulib_amd64_init (mulib_amd64_t *context);
  /** @brief Boolean initialization with a refreshed optional diagnostic. */
  int mulib_amd64_init_ex (mulib_amd64_t *context, mulib_archml_error_t *error);
  /** @brief Release/reset both owners. NULL/repeated free permitted. */
  void mulib_amd64_free (mulib_amd64_t *context);
  /** @brief Legacy typed mnemonic API: success is MULIB_ASSEMBLER_OK (0).
   * Preserves image/linker on failure and refreshes error even for bad context. */
  int mulib_amd64_emit (const mulib_amd64_t *context, mulib_emitter_t *image,
                        mulib_linker_t *linker, const char *mnemonic,
                        const mulib_assembler_operand_t *operands, size_t count,
                         mulib_assembler_error_t *error);
  /** @brief Borrow the MPU contract, or NULL when not initialized. */
  const mulib_mpu_t *mulib_amd64_mpu (const mulib_amd64_t *context);
  /** @brief Encode a concrete ArchML form; success is MULIB_ARCHML_OK (1).
   * Includes memory, SIMD and extended forms present in the profile. */
  mulib_archml_result mulib_amd64_encode (const mulib_amd64_t *, const char *,
      const uint64_t *, size_t, void *, size_t, size_t *);
  /** @brief Transactional named-form decoding; success is 1. */
  mulib_archml_result mulib_amd64_decode (const mulib_amd64_t *, const char *,
      const void *, size_t, uint64_t *, size_t, size_t *);
  /** @brief Append a concrete form with numeric operands; success is 1.
   * No labels/relocations; use legacy emit for symbolic branches. */
  mulib_archml_result mulib_amd64_emit_form (const mulib_amd64_t *,
      mulib_emitter_t *, const char *, const uint64_t *, size_t,
      mulib_archml_error_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_AMD64) && !defined(MULIB_AMD64_IMPLEMENTED)
#define MULIB_AMD64_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int
mulib_amd64_init (mulib_amd64_t *c)
{ return mulib_amd64_init_ex (c, NULL); }
int
mulib_amd64_init_ex (mulib_amd64_t *c, mulib_archml_error_t *error)
{
  mulib_amd64_t next = { 0 };
  if (error) {
    error->code = MULIB_ARCHML_INVALID; error->offset = 0;
    error->line = error->column = 1; error->message = "invalid or live AMD64 context";
  }
  if (!c || c->specification || c->mpu.document) return 0;
  if (mulib_mpu_init (&next.mpu, "amd64", error) != MULIB_ARCHML_OK) return 0;
  next.specification = mulib_dsymspec_load_builtin ("amd64", NULL);
  if (!next.specification) {
    mulib_mpu_free (&next.mpu);
    if (error) { error->code = MULIB_ARCHML_NOMEM; error->message = "AMD64 dsymspec load failed"; }
    return 0;
  }
  *c = next;
  return 1;
}
void
mulib_amd64_free (mulib_amd64_t *c)
{
  if (c)
    {
      mulib_dsymspec_destroy (c->specification);
      c->specification = NULL;
      mulib_mpu_free (&c->mpu);
    }
}
int
mulib_amd64_emit (const mulib_amd64_t *c, mulib_emitter_t *image,
                  mulib_linker_t *linker, const char *name,
                  const mulib_assembler_operand_t *operands, size_t n,
                  mulib_assembler_error_t *error)
{
  if (!c || !c->specification)
    {
      if (error) { error->message = "uninitialized AMD64 context"; error->linker_result = 0; }
      return MULIB_ASSEMBLER_EINVAL;
    }
  return mulib_assembler_emit (c->specification, image, linker, name, operands,
                               n, error);
}
const mulib_mpu_t *mulib_amd64_mpu (const mulib_amd64_t *c)
{ return c && c->mpu.document ? &c->mpu : NULL; }
mulib_archml_result mulib_amd64_encode (const mulib_amd64_t *c, const char *name,
    const uint64_t *values, size_t count, void *bytes, size_t capacity, size_t *length)
{ return mulib_mpu_encode (mulib_amd64_mpu (c), name, values, count, bytes, capacity, length); }
mulib_archml_result mulib_amd64_decode (const mulib_amd64_t *c, const char *name,
    const void *bytes, size_t length, uint64_t *values, size_t count, size_t *consumed)
{ return mulib_mpu_decode (mulib_amd64_mpu (c), name, bytes, length, values, count, consumed); }
mulib_archml_result mulib_amd64_emit_form (const mulib_amd64_t *c, mulib_emitter_t *image,
    const char *name, const uint64_t *values, size_t count, mulib_archml_error_t *error)
{ return mulib_mpu_emit (mulib_amd64_mpu (c), image, name, values, count, error); }
#ifdef __cplusplus
}
#endif
#endif
