/* MuLIB: Owned named record fields with deep-copied datum values. C11 single-header;
 * define MULIB_IMPL_RECORD once. */
#ifndef MULIB_RECORD_H
#define MULIB_RECORD_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-datum.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    char *name;
    mulib_datum_t value;
  } mulib_record_field_t;
  typedef struct
  {
    mulib_record_field_t *fields;
    size_t count, capacity;
  } mulib_record_t;
  const mulib_datum_t *mulib_record_get (const mulib_record_t *r, const char *name);
  int mulib_record_set (mulib_record_t *r, const char *name,
                        const mulib_datum_t *value);
  int mulib_record_clone (mulib_record_t *dst, const mulib_record_t *src);
  void mulib_record_free (mulib_record_t *r);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RECORD) && !defined(MULIB_RECORD_IMPLEMENTED)
#define MULIB_RECORD_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  const mulib_datum_t *
  mulib_record_get (const mulib_record_t *r, const char *name)
  {
    if (!r || !name)
      return NULL;
    for (size_t i = 0; i < r->count; i++)
      if (!strcmp (r->fields[i].name, name))
        return &r->fields[i].value;
    return NULL;
  }
  int
  mulib_record_set (mulib_record_t *r, const char *name, const mulib_datum_t *value)
  {
    if (!r || !name || !*name || !value)
      return 0;
    for (size_t i = 0; i < r->count; i++)
      if (!strcmp (r->fields[i].name, name))
        return mulib_datum_clone (&r->fields[i].value, value);
    size_t l = strlen (name);
    if (l == SIZE_MAX)
      return 0;
    char *s = (char *)malloc (l + 1);
    if (!s)
      return 0;
    memcpy (s, name, l + 1);
    mulib_datum_t v = { 0 };
    if (!mulib_datum_clone (&v, value))
      {
        free (s);
        return 0;
      }
    if (r->count == r->capacity)
      {
        size_t cap = r->capacity ? r->capacity * 2 : 8;
        if (cap < r->capacity || cap > SIZE_MAX / sizeof (*r->fields))
          {
            free (s);
            mulib_datum_free (&v);
            return 0;
          }
        void *p = realloc (r->fields, cap * sizeof (*r->fields));
        if (!p)
          {
            free (s);
            mulib_datum_free (&v);
            return 0;
          }
        r->fields = (mulib_record_field_t *)p;
        r->capacity = cap;
      }
    r->fields[r->count].name = s;
    r->fields[r->count].value = v;
    r->count++;
    return 1;
  }
  void
  mulib_record_free (mulib_record_t *r)
  {
    if (!r)
      return;
    for (size_t i = 0; i < r->count; i++)
      {
        free (r->fields[i].name);
        mulib_datum_free (&r->fields[i].value);
      }
    free (r->fields);
    memset (r, 0, sizeof (*r));
  }
  int
  mulib_record_clone (mulib_record_t *dst, const mulib_record_t *src)
  {
    if (!dst || !src)
      return 0;
    if (dst == src)
      return 1;
    mulib_record_t t = { 0 };
    for (size_t i = 0; i < src->count; i++)
      if (!mulib_record_set (&t, src->fields[i].name, &src->fields[i].value))
        {
          mulib_record_free (&t);
          return 0;
        }
    mulib_record_free (dst);
    *dst = t;
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
