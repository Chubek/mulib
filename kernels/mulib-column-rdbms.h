/** @file mulib-column-rdbms.h
 * @brief Dense column-oriented finite-double table with atomic row append.
 * Define MULIB_IMPL_COLUMN_RDBMS. No SQL, nulls or persistence. Zero
 * initialize, set nonzero columns before first append; never change columns
 * while live. */
#ifndef MULIB_COLUMN_RDBMS_H
#define MULIB_COLUMN_RDBMS_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_column_rdbms
  {
    double *data;
    size_t columns, rows;
  } mulib_column_rdbms_t;
  int mulib_column_rdbms_append (mulib_column_rdbms_t *table, const double *row,
                                 size_t count);
  /** Borrow contiguous column of rows elements until mutation/free. */
  const double *mulib_column_rdbms_column (const mulib_column_rdbms_t *table,
                                           size_t column);
  void mulib_column_rdbms_free (mulib_column_rdbms_t *table);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_COLUMN_RDBMS) && !defined(MULIB_COLUMN_RDBMS_IMPLEMENTED)
#define MULIB_COLUMN_RDBMS_IMPLEMENTED
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
int
mulib_column_rdbms_append (mulib_column_rdbms_t *t, const double *row, size_t n)
{
  double *p;
  size_t rows;
  if (!t || !row || !t->columns || n != t->columns || t->rows == SIZE_MAX
      || (t->rows && !t->data))
    return 0;
  rows = t->rows + 1;
  if (rows > SIZE_MAX / sizeof (double) / n)
    return 0;
  for (size_t i = 0; i < n; ++i)
    if (!isfinite (row[i]))
      return 0;
  p = (double *)malloc (rows * n * sizeof (double));
  if (!p)
    return 0;
  for (size_t i = 0; i < n; ++i)
    {
      if (t->rows)
        memcpy (p + i * rows, t->data + i * t->rows, t->rows * sizeof (double));
      p[i * rows + t->rows] = row[i];
    }
  free (t->data);
  t->data = p;
  t->rows = rows;
  return 1;
}
const double *
mulib_column_rdbms_column (const mulib_column_rdbms_t *t, size_t c)
{
  return t && c < t->columns && t->rows && t->data ? t->data + c * t->rows
                                                   : NULL;
}
void
mulib_column_rdbms_free (mulib_column_rdbms_t *t)
{
  if (t)
    {
      free (t->data);
      memset (t, 0, sizeof (*t));
    }
}
#endif
