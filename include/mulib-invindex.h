/** @file mulib-invindex.h
 * @brief Binary term to sorted unique uint64 posting lists over memtable
 * storage. Define MULIB_IMPL_INVINDEX plus MEMTABLE, BUFFER and BYCUTILS. */
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
  /** @brief Zero init. Add a posting idempotently; 0 success, negative error.
   */
  int mulib_invindex_add (mulib_invindex_t *t, const void *term, size_t size,
                          uint64_t document);
  /** @brief Enumerate one posting by ascending index; 0 found, 1
   * absent/out-of-range. */
  int mulib_invindex_at (const mulib_invindex_t *t, const void *term,
                         size_t size, size_t index, uint64_t *document);
  /** @brief Remove posting; 0 removed, 1 absent, negative error. */
  int mulib_invindex_remove (mulib_invindex_t *t, const void *term,
                             size_t size, uint64_t document);
  /** @brief Release/reset all terms/postings. */
  void mulib_invindex_free (mulib_invindex_t *t);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_INVINDEX) && !defined(MULIB_INVINDEX_IMPLEMENTED)
#define MULIB_INVINDEX_IMPLEMENTED
#include "mulib-buffer.h"
#include "mulib-bycutils.h"
static int
mulib_invindex_edit_ (mulib_invindex_t *t, const void *k, size_t n,
                      uint64_t doc, int remove)
{
  const mulib_memtable_record_t *r;
  mulib_buffer_t b = { 0 };
  size_t i, pos = 0, len = 0;
  uint64_t v = 0;
  int rc, found = 0;
  if (!t || (!k && n))
    return -1;
  rc = mulib_memtable_get (&t->terms, k, n, &r);
  if (rc < 0)
    return rc;
  if (!rc)
    {
      len = r->value_size;
      for (i = 0; i < len; i += 8)
        {
          mulib_bycutils_load (r->value, len, i, 8, &v);
          if (v >= doc)
            {
              found = v == doc;
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
      mulib_bycutils_store (raw, 8, 0, 8, doc);
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
mulib_invindex_remove (mulib_invindex_t *t, const void *k, size_t n,
                       uint64_t doc)
{
  return mulib_invindex_edit_ (t, k, n, doc, 1);
}
int
mulib_invindex_at (const mulib_invindex_t *t, const void *k, size_t n,
                   size_t i, uint64_t *out)
{
  const mulib_memtable_record_t *r;
  int rc;
  if (!t || !out)
    return -1;
  rc = mulib_memtable_get (&t->terms, k, n, &r);
  if (rc)
    return rc;
  if (i >= r->value_size / 8)
    return 1;
  return mulib_bycutils_load (r->value, r->value_size, i * 8, 8, out) ? 0 : -1;
}
void
mulib_invindex_free (mulib_invindex_t *t)
{
  if (t)
    mulib_memtable_free (&t->terms);
}
#endif
