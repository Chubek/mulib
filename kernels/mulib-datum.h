/* MuLIB: Owned typed scalar values with binary-safe strings, deep-copy semantics and
 * NULL. C11 single-header; define MULIB_IMPL_DATUM once. */
#ifndef MULIB_DATUM_H
#define MULIB_DATUM_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum
  {
    MULIB_DATUM_NULL,
    MULIB_DATUM_I64,
    MULIB_DATUM_F64,
    MULIB_DATUM_BOOL,
    MULIB_DATUM_BYTES
  } mulib_datum_kind_t;
  typedef struct
  {
    mulib_datum_kind_t kind;
    union
    {
      int64_t integer;
      double real;
      int boolean;
      struct
      {
        unsigned char *data;
        size_t length;
      } bytes;
    } value;
  } mulib_datum_t;
  void mulib_datum_init (mulib_datum_t *v);
  int mulib_datum_set_i64 (mulib_datum_t *v, int64_t x);
  int mulib_datum_set_f64 (mulib_datum_t *v, double x);
  int mulib_datum_set_bytes (mulib_datum_t *v, const void *bytes, size_t len);
  int mulib_datum_clone (mulib_datum_t *dst, const mulib_datum_t *src);
  void mulib_datum_free (mulib_datum_t *v);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DATUM) && !defined(MULIB_DATUM_IMPLEMENTED)
#define MULIB_DATUM_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  void
  mulib_datum_init (mulib_datum_t *v)
  {
    if (v)
      memset (v, 0, sizeof (*v));
  }
  void
  mulib_datum_free (mulib_datum_t *v)
  {
    if (!v)
      return;
    if (v->kind == MULIB_DATUM_BYTES)
      free (v->value.bytes.data);
    memset (v, 0, sizeof (*v));
  }
  int
  mulib_datum_set_i64 (mulib_datum_t *v, int64_t x)
  {
    if (!v)
      return 0;
    mulib_datum_free (v);
    v->kind = MULIB_DATUM_I64;
    v->value.integer = x;
    return 1;
  }
  int
  mulib_datum_set_f64 (mulib_datum_t *v, double x)
  {
    if (!v)
      return 0;
    mulib_datum_free (v);
    v->kind = MULIB_DATUM_F64;
    v->value.real = x;
    return 1;
  }
  int
  mulib_datum_set_bytes (mulib_datum_t *v, const void *s, size_t n)
  {
    unsigned char *p = NULL;
    if (!v || (!s && n) || n == SIZE_MAX)
      return 0;
    if (n)
      {
        p = (unsigned char *)malloc (n);
        if (!p)
          return 0;
        memcpy (p, s, n);
      }
    mulib_datum_free (v);
    v->kind = MULIB_DATUM_BYTES;
    v->value.bytes.data = p;
    v->value.bytes.length = n;
    return 1;
  }
  int
  mulib_datum_clone (mulib_datum_t *dst, const mulib_datum_t *src)
  {
    if (!dst || !src)
      return 0;
    if (dst == src)
      return 1;
    if (src->kind == MULIB_DATUM_BYTES)
      return mulib_datum_set_bytes (dst, src->value.bytes.data,
                                    src->value.bytes.length);
    mulib_datum_free (dst);
    *dst = *src;
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
