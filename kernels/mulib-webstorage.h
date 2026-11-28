/** @file mulib-webstorage.h
 * @brief In-memory Web Storage style string key-value store.
 * C11 single-header implementation. Define MULIB_IMPL_WEBSTORAGE in one
 * translation unit.
 */
#ifndef MULIB_WEBSTORAGE_H
#define MULIB_WEBSTORAGE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_webstorage_entry
  {
    char *key, *value;
  } mulib_webstorage_entry_t;
  typedef struct mulib_webstorage
  {
    mulib_webstorage_entry_t *entries;
    size_t len, cap;
  } mulib_webstorage_t;
  /** @brief Copy strings into zero-initialized storage. 1 success, 0 failure;
   * failures preserve entries. Empty keys/values and aliases are supported. */
  int mulib_webstorage_set (mulib_webstorage_t *s, const char *key,
                            const char *value);
  /** @brief As set, but enforce literal entry and string-byte limits on the
   * resulting store. Count one NUL per key/value; exclude array overhead. */
  int mulib_webstorage_set_limited (mulib_webstorage_t *s, const char *key,
                                    const char *value, size_t max_entries,
                                    size_t max_string_bytes);
  /** @brief Borrow a value until replacement, removal, clear or free; NULL absent.
   */
  const char *mulib_webstorage_get (const mulib_webstorage_t *s,
                                    const char *key);
  /** @brief Borrow a key by insertion order; NULL out of range. */
  const char *mulib_webstorage_key (const mulib_webstorage_t *s, size_t index);
  /** @brief Remove an exact key; return 1 if removed, 0 for miss/invalid. */
  int mulib_webstorage_remove (mulib_webstorage_t *s, const char *key);
  /** @brief Remove all entries but keep capacity for reuse; NULL is safe. */
  void mulib_webstorage_clear (mulib_webstorage_t *s);
  /** @brief Release all entries/reset state; NULL and repeated calls safe. */
  void mulib_webstorage_free (mulib_webstorage_t *s);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WEBSTORAGE) && !defined(MULIB_WEBSTORAGE_IMPLEMENTED)
#define MULIB_WEBSTORAGE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  static char *
  mulib_webstorage_dup_ (const char *s)
  {
    size_t n = strlen (s) + 1;
    char *p = (char *)malloc (n);
    if (p)
      memcpy (p, s, n);
    return p;
  }

  static size_t
  mulib_webstorage_find_ (const mulib_webstorage_t *s, const char *key)
  {
    for (size_t i = 0; i < s->len; i++)
      if (!strcmp (s->entries[i].key, key))
        return i;
    return s->len;
  }

  const char *
  mulib_webstorage_get (const mulib_webstorage_t *s, const char *key)
  {
    if (!s || !key)
      return NULL;
    size_t i = mulib_webstorage_find_ (s, key);
    return i < s->len ? s->entries[i].value : NULL;
  }

  static int
  mulib_webstorage_set_ (mulib_webstorage_t *s, size_t i, const char *key,
                         const char *value)
  {
    char *v = mulib_webstorage_dup_ (value);
    if (!v)
      return 0;
    if (i < s->len)
      {
        free (s->entries[i].value);
        s->entries[i].value = v;
        return 1;
      }
    char *k = mulib_webstorage_dup_ (key);
    if (!k)
      {
        free (v);
        return 0;
      }
    if (s->len == s->cap)
      {
        if (s->cap > SIZE_MAX / 2)
          {
            free (k);
            free (v);
            return 0;
          }
        size_t c = s->cap ? s->cap * 2 : 8;
        if (c > SIZE_MAX / sizeof (*s->entries))
          {
            free (k);
            free (v);
            return 0;
          }
        void *p = realloc (s->entries, c * sizeof (*s->entries));
        if (!p)
          {
            free (k);
            free (v);
            return 0;
          }
        s->entries = (mulib_webstorage_entry_t *)p;
        s->cap = c;
      }
    s->entries[s->len].key = k;
    s->entries[s->len++].value = v;
    return 1;
  }

  int
  mulib_webstorage_set (mulib_webstorage_t *s, const char *key,
                        const char *value)
  {
    if (!s || !key || !value)
      return 0;
    return mulib_webstorage_set_ (s, mulib_webstorage_find_ (s, key), key,
                                  value);
  }

  /* Include each terminating NUL without an overflowing accumulator. */
  static int
  mulib_webstorage_charge_ (size_t *left, const char *s)
  {
    while (*left)
      {
        --*left;
        if (!*s++)
          return 1;
      }
    return 0;
  }

  int
  mulib_webstorage_set_limited (mulib_webstorage_t *s, const char *key,
                                const char *value, size_t max_entries,
                                size_t max_string_bytes)
  {
    if (!s || !key || !value || s->len > max_entries
        || !mulib_webstorage_charge_ (&max_string_bytes, key)
        || !mulib_webstorage_charge_ (&max_string_bytes, value))
      return 0;
    size_t i = mulib_webstorage_find_ (s, key);
    if (i == s->len && s->len == max_entries)
      return 0;
    for (size_t j = 0; j < s->len; j++)
      if (j != i
          && (!mulib_webstorage_charge_ (&max_string_bytes, s->entries[j].key)
              || !mulib_webstorage_charge_ (&max_string_bytes,
                                           s->entries[j].value)))
        return 0;
    return mulib_webstorage_set_ (s, i, key, value);
  }

  const char *
  mulib_webstorage_key (const mulib_webstorage_t *s, size_t index)
  {
    return s && index < s->len ? s->entries[index].key : NULL;
  }

  int
  mulib_webstorage_remove (mulib_webstorage_t *s, const char *key)
  {
    if (!s || !key)
      return 0;
    size_t i = mulib_webstorage_find_ (s, key);
    if (i == s->len)
      return 0;
    free (s->entries[i].key);
    free (s->entries[i].value);
    if (i + 1 < s->len)
      memmove (s->entries + i, s->entries + i + 1,
               (s->len - i - 1) * sizeof *s->entries);
    s->entries[--s->len].key = NULL;
    s->entries[s->len].value = NULL;
    return 1;
  }

  void
  mulib_webstorage_clear (mulib_webstorage_t *s)
  {
    if (!s)
      return;
    for (size_t i = 0; i < s->len; i++)
      {
        free (s->entries[i].key);
        free (s->entries[i].value);
        s->entries[i].key = NULL;
        s->entries[i].value = NULL;
      }
    s->len = 0;
  }

  void
  mulib_webstorage_free (mulib_webstorage_t *s)
  {
    if (!s)
      return;
    mulib_webstorage_clear (s);
    free (s->entries);
    memset (s, 0, sizeof *s);
  }

#ifdef __cplusplus
}
#endif
#endif
