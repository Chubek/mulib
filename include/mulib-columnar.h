/* MuLIB: Nullable column-major finite double vectors with atomic append. C11
 * single-header; define MULIB_IMPL_COLUMNAR once. */
#ifndef MULIB_COLUMNAR_H
#define MULIB_COLUMNAR_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    double *values;
    unsigned char *valid;
    size_t count, capacity;
  } mulib_columnar_t;
  /* Each append stores numeric value or null when valid==0. */
  int mulib_columnar_append (mulib_columnar_t *c, double value, int valid);
  int mulib_columnar_get (const mulib_columnar_t *c, size_t index, double *value,
                          int *valid);
  void mulib_columnar_free (mulib_columnar_t *c);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_COLUMNAR) && !defined(MULIB_COLUMNAR_IMPLEMENTED)
#define MULIB_COLUMNAR_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_columnar_append (mulib_columnar_t *c, double x, int valid)
  {
    if (!c || (valid && !isfinite (x)))
      return 0;
    if (c->count == c->capacity)
      {
        size_t nc = c->capacity ? c->capacity * 2 : 16;
        if (nc < c->capacity || nc > SIZE_MAX / sizeof (double))
          return 0;
        double *v = (double *)malloc (nc * sizeof (double));
        unsigned char *bits = (unsigned char *)malloc (nc);
        if (!v || !bits)
          {
            free (v);
            free (bits);
            return 0;
          }
        if (c->count)
          {
            memcpy (v, c->values, c->count * sizeof (double));
            memcpy (bits, c->valid, c->count);
          }
        free (c->values);
        free (c->valid);
        c->values = v;
        c->valid = bits;
        c->capacity = nc;
      }
    c->values[c->count] = valid ? x : 0;
    c->valid[c->count++] = (unsigned char)!!valid;
    return 1;
  }
  int
  mulib_columnar_get (const mulib_columnar_t *c, size_t i, double *v, int *valid)
  {
    if (!c || i >= c->count)
      return 0;
    if (v)
      *v = c->values[i];
    if (valid)
      *valid = c->valid[i];
    return 1;
  }
  void
  mulib_columnar_free (mulib_columnar_t *c)
  {
    if (!c)
      return;
    free (c->values);
    free (c->valid);
    memset (c, 0, sizeof (*c));
  }
#ifdef __cplusplus
}
#endif
#endif
