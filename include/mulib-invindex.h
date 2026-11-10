/** @file mulib-invindex.h
 * @brief Patched owned sorted document postings with count and AND-intersection.
 * Define MULIB_IMPL_INVINDEX plus MEMTABLE, BUFFER, BYCUTILS. */
#ifndef MULIB_INVINDEX_H
#define MULIB_INVINDEX_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_invindex
  {
    mulib_memtable_t terms;
  } mulib_invindex_t;
  int mulib_invindex_add (mulib_invindex_t *t, const void *term, size_t size,
                          uint64_t doc);
  int mulib_invindex_at (const mulib_invindex_t *t, const void *term, size_t size,
                         size_t index, uint64_t *doc);
  int mulib_invindex_remove (mulib_invindex_t *t, const void *term, size_t size,
                             uint64_t doc);
  void mulib_invindex_free (mulib_invindex_t *t);
  /* New: number of postings; returns SIZE_MAX on error, zero when absent. */
  size_t mulib_invindex_count (const mulib_invindex_t *t, const void *term,
                               size_t size);
  /* New: document-ID intersection of two terms; returns SIZE_MAX on error or
   * inadequate capacity. On success out is ascending unique IDs. */
  size_t mulib_invindex_intersect (const mulib_invindex_t *t, const void *a, size_t an,
                                   const void *b, size_t bn, uint64_t *out,
                                   size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_INVINDEX) && !defined(MULIB_INVINDEX_IMPLEMENTED)
#define MULIB_INVINDEX_IMPLEMENTED
#include "mulib-buffer.h"
#include "mulib-bycutils.h"
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_invindex_edit_ (mulib_invindex_t *t, const void *k, size_t n, uint64_t doc,
                        int remove)
  {
    const mulib_memtable_record_t *r = NULL;
    mulib_buffer_t b = { 0 };
    size_t i, pos = 0, len = 0;
    uint64_t value = 0;
    int rc, found = 0;
    if (!t || (!k && n))
      return -1;
    rc = mulib_memtable_get (&t->terms, k, n, &r);
    if (rc < 0)
      return rc;
    if (!rc)
      {
        len = r->value_size;
        if (len % 8)
          return -1;
        for (i = 0; i < len; i += 8)
          {
            if (!mulib_bycutils_load (r->value, len, i, 8, &value))
              return -1;
            if (value >= doc)
              {
                found = value == doc;
                break;
              }
            pos += 8;
          }
      }
    if (remove && !found)
      return 1;
    if (!remove && found)
      return 0;
    if (len && !mulib_buffer_append (&b, r->value, len))
      return -2;
    if (remove)
      mulib_buffer_remove (&b, pos, 8);
    else
      {
        unsigned char raw[8];
        if (!mulib_bycutils_store (raw, 8, 0, 8, doc))
          {
            mulib_buffer_free (&b);
            return -1;
          }
        if (!mulib_buffer_append (&b, raw, 8))
          {
            mulib_buffer_free (&b);
            return -2;
          }
        memmove (b.data + pos + 8, b.data + pos, len - pos);
        memcpy (b.data + pos, raw, 8);
      }
    rc = mulib_memtable_put (&t->terms, k, n, b.data, b.length, 0, 0);
    mulib_buffer_free (&b);
    return rc;
  }
  int
  mulib_invindex_add (mulib_invindex_t *t, const void *k, size_t n, uint64_t doc)
  {
    return mulib_invindex_edit_ (t, k, n, doc, 0);
  }
  int
  mulib_invindex_remove (mulib_invindex_t *t, const void *k, size_t n, uint64_t doc)
  {
    return mulib_invindex_edit_ (t, k, n, doc, 1);
  }
  int
  mulib_invindex_at (const mulib_invindex_t *t, const void *k, size_t n, size_t i,
                     uint64_t *out)
  {
    const mulib_memtable_record_t *r = NULL;
    if (!t || !out || (!k && n))
      return -1;
    int rc = mulib_memtable_get (&t->terms, k, n, &r);
    if (rc)
      return rc;
    if (r->value_size % 8)
      return -1;
    if (i >= r->value_size / 8)
      return 1;
    return mulib_bycutils_load (r->value, r->value_size, i * 8, 8, out) ? 0 : -1;
  }
  size_t
  mulib_invindex_count (const mulib_invindex_t *t, const void *k, size_t n)
  {
    const mulib_memtable_record_t *r = NULL;
    if (!t || (!k && n))
      return SIZE_MAX;
    int rc = mulib_memtable_get (&t->terms, k, n, &r);
    if (rc == 1)
      return 0;
    if (rc || r->value_size % 8)
      return SIZE_MAX;
    return r->value_size / 8;
  }
  size_t
  mulib_invindex_intersect (const mulib_invindex_t *t, const void *a, size_t an,
                            const void *b, size_t bn, uint64_t *out, size_t cap)
  {
    if (!t || (!a && an) || (!b && bn) || (!out && cap))
      return SIZE_MAX;
    size_t n1 = mulib_invindex_count (t, a, an), n2 = mulib_invindex_count (t, b, bn);
    if (n1 == SIZE_MAX || n2 == SIZE_MAX)
      return SIZE_MAX;
    size_t matches = 0;
    for (int pass = 0; pass < 2; pass++)
      {
        size_t i = 0, j = 0, at = 0;
        while (i < n1 && j < n2)
          {
            uint64_t x, y;
            if (mulib_invindex_at (t, a, an, i, &x)
                || mulib_invindex_at (t, b, bn, j, &y))
              return SIZE_MAX;
            if (x < y)
              i++;
            else if (x > y)
              j++;
            else
              {
                if (pass)
                  out[at] = x;
                at++;
                i++;
                j++;
              }
          }
        if (!pass)
          {
            matches = at;
            if (matches > cap)
              return SIZE_MAX;
          }
      }
    return matches;
  }
  void
  mulib_invindex_free (mulib_invindex_t *t)
  {
    if (t)
      mulib_memtable_free (&t->terms);
  }
#ifdef __cplusplus
}
#endif
#endif
