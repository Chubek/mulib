/* MuLIB: Borrowed tab-separated field iterator preserving empty fields. C11
 * single-header; define MULIB_IMPL_TSV once. */
#ifndef MULIB_TSV_H
#define MULIB_TSV_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    const char *row;
    size_t length, pos;
    int done;
  } mulib_tsv_t;
  void mulib_tsv_init (mulib_tsv_t *t, const char *row, size_t length);
  int mulib_tsv_next (mulib_tsv_t *t, const char **field, size_t *field_length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TSV) && !defined(MULIB_TSV_IMPLEMENTED)
#define MULIB_TSV_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  void
  mulib_tsv_init (mulib_tsv_t *t, const char *s, size_t n)
  {
    if (!t)
      return;
    t->row = s;
    t->length = s ? n : 0;
    t->pos = 0;
    t->done = 0;
  }
  int
  mulib_tsv_next (mulib_tsv_t *t, const char **f, size_t *n)
  {
    if (!t || !f || !n || t->done)
      return 0;
    size_t start = t->pos, p = start;
    while (p < t->length && t->row[p] != '	')
      p++;
    *f = t->row ? t->row + start : NULL;
    *n = p - start;
    if (p < t->length)
      t->pos = p + 1;
    else
      t->done = 1;
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
