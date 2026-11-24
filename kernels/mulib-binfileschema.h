/** @file mulib-binfileschema.h
 * @brief Fixed-record schemas for named 1..8 byte little-endian integer
 * fields. Define MULIB_IMPL_BINFILESCHEMA plus BYCUTILS. Schemas borrow
 * immutable fields. */
#ifndef MULIB_BINFILESCHEMA_H
#define MULIB_BINFILESCHEMA_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_binfileschema_field
  {
    const char *name;
    size_t offset;
    unsigned width;
  } mulib_binfileschema_field_t;
  typedef struct mulib_binfileschema
  {
    const mulib_binfileschema_field_t *fields;
    size_t count, record_size;
  } mulib_binfileschema_t;
  /** @brief Require unique nonempty names, valid widths/ranges and no overlap.
   */
  int mulib_binfileschema_validate (const mulib_binfileschema_t *schema);
  /** @brief Decode named field from a complete record; 1 success, unchanged on
   * error. */
  int mulib_binfileschema_get (const mulib_binfileschema_t *schema,
                               const void *record, size_t size,
                               const char *name, uint64_t *value);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BINFILESCHEMA)                                         \
    && !defined(MULIB_BINFILESCHEMA_IMPLEMENTED)
#define MULIB_BINFILESCHEMA_IMPLEMENTED
#include "mulib-bycutils.h"
#include <string.h>
int
mulib_binfileschema_validate (const mulib_binfileschema_t *s)
{
  size_t i, j;
  if (!s || (!s->fields && s->count) || !s->record_size)
    return 0;
  for (i = 0; i < s->count; ++i)
    {
      const mulib_binfileschema_field_t *f = s->fields + i;
      if (!f->name || !*f->name || !f->width || f->width > 8
          || !mulib_bycutils_range (s->record_size, f->offset, f->width))
        return 0;
      for (j = 0; j < i; ++j)
        {
          const mulib_binfileschema_field_t *g = s->fields + j;
          if (!strcmp (f->name, g->name)
              || (f->offset < g->offset + g->width
                  && g->offset < f->offset + f->width))
            return 0;
        }
    }
  return 1;
}
int
mulib_binfileschema_get (const mulib_binfileschema_t *s, const void *p,
                         size_t n, const char *name, uint64_t *out)
{
  size_t i;
  if (!name || !mulib_binfileschema_validate (s) || n < s->record_size)
    return 0;
  for (i = 0; i < s->count; ++i)
    if (!strcmp (name, s->fields[i].name))
      return mulib_bycutils_load (p, n, s->fields[i].offset,
                                  s->fields[i].width, out);
  return 0;
}
#endif
