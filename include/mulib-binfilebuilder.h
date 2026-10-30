/** @file mulib-binfilebuilder.h
 * @brief Transactional fixed-schema record encoding into shared byte buffers.
 * Define MULIB_IMPL_BINFILEBUILDER plus BINFILESCHEMA, BYCUTILS and BUFFER. */
#ifndef MULIB_BINFILEBUILDER_H
#define MULIB_BINFILEBUILDER_H
#include "mulib-binfileschema.h"
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Append zero-padded record, values in schema field order. All
   * fields required; count must match. 1 success, 0 invalid/range/OOM, output
   * unchanged. */
  int mulib_binfilebuilder_record (mulib_buffer_t *out,
                                   const mulib_binfileschema_t *schema,
                                   const uint64_t *values, size_t count);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BINFILEBUILDER)                                        \
    && !defined(MULIB_BINFILEBUILDER_IMPLEMENTED)
#define MULIB_BINFILEBUILDER_IMPLEMENTED
#include "mulib-bycutils.h"
int
mulib_binfilebuilder_record (mulib_buffer_t *b, const mulib_binfileschema_t *s,
                             const uint64_t *v, size_t n)
{
  unsigned char *p;
  size_t i;
  int ok = 1;
  if (!b || (!v && n) || !mulib_binfileschema_validate (s) || n != s->count)
    return 0;
  p = (unsigned char *)calloc (s->record_size, 1);
  if (!p)
    return 0;
  for (i = 0; i < n && ok; ++i)
    ok = mulib_bycutils_store (p, s->record_size, s->fields[i].offset,
                               s->fields[i].width, v[i]);
  if (ok)
    ok = mulib_buffer_append (b, p, s->record_size);
  free (p);
  return ok;
}
#endif
