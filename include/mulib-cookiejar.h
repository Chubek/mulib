/** @file mulib-cookiejar.h
 * @brief Owned in-memory cookie-pair store with name replacement.
 * C11 single-header implementation. Define MULIB_IMPL_COOKIEJAR in one
 * translation unit.
 */
#ifndef MULIB_COOKIEJAR_H
#define MULIB_COOKIEJAR_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_cookiejar
  {
    char **names, **values;
    size_t length, capacity;
  } mulib_cookiejar_t;
  /** @brief Copy a nonempty name and value into a zero-initialized pair store.
   * No domain/path/expiry/security policy or cookie validation is performed.
   * 1 success, 0 failure; stored pairs unchanged on failure. Aliases allowed.
   */
  int mulib_cookiejar_set (mulib_cookiejar_t *j, const char *name,
                           const char *value);
  /** @brief Borrow a value until its replacement or free; NULL absent. */
  const char *mulib_cookiejar_get (const mulib_cookiejar_t *j,
                                   const char *name);
  /** @brief Release all pairs/reset state; NULL and repeated calls are safe. */
  void mulib_cookiejar_free (mulib_cookiejar_t *j);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_COOKIEJAR) && !defined(MULIB_COOKIEJAR_IMPLEMENTED)
#define MULIB_COOKIEJAR_IMPLEMENTED
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  static char *
  mulib_cookiejar_dup_ (const char *s)
  {
    size_t n = strlen (s) + 1;
    char *p = (char *)malloc (n);
    if (p)
      memcpy (p, s, n);
    return p;
  }
  const char *
  mulib_cookiejar_get (const mulib_cookiejar_t *j, const char *n)
  {
    if (!j || !n)
      return NULL;
    for (size_t i = 0; i < j->length; i++)
      if (!strcmp (j->names[i], n))
        return j->values[i];
    return NULL;
  }
  int
  mulib_cookiejar_set (mulib_cookiejar_t *j, const char *n, const char *v)
  {
    if (!j || !n || !v || !*n)
      return 0;
    char *nv = mulib_cookiejar_dup_ (v);
    if (!nv)
      return 0;
    for (size_t i = 0; i < j->length; i++)
      if (!strcmp (j->names[i], n))
        {
          free (j->values[i]);
          j->values[i] = nv;
          return 1;
        }
    char *nn = mulib_cookiejar_dup_ (n);
    if (!nn)
      {
        free (nv);
        return 0;
      }
    if (j->length == j->capacity)
      {
        size_t c = j->capacity ? j->capacity * 2 : 8;
        if (c <= j->capacity || c > SIZE_MAX / sizeof (char *))
          {
            free (nn);
            free (nv);
            return 0;
          }
        char **a = (char **)malloc (c * sizeof (char *)),
             **b = (char **)malloc (c * sizeof (char *));
        if (!a || !b)
          {
            free (a);
            free (b);
            free (nn);
            free (nv);
            return 0;
          }
        if (j->length)
          {
            memcpy (a, j->names, j->length * sizeof (char *));
            memcpy (b, j->values, j->length * sizeof (char *));
          }
        free (j->names);
        free (j->values);
        j->names = a;
        j->values = b;
        j->capacity = c;
      }
    j->names[j->length] = nn;
    j->values[j->length++] = nv;
    return 1;
  }
  void
  mulib_cookiejar_free (mulib_cookiejar_t *j)
  {
    if (!j)
      return;
    for (size_t i = 0; i < j->length; i++)
      {
        free (j->names[i]);
        free (j->values[i]);
      }
    free (j->names);
    free (j->values);
    memset (j, 0, sizeof *j);
  }

#ifdef __cplusplus
}
#endif
#endif
