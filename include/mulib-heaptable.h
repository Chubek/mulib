/** @file mulib-heaptable.h
 * @brief Append-ID binary row storage over the shared memtable.
 * Define MULIB_IMPL_HEAPTABLE and MULIB_IMPL_MEMTABLE once. */
#ifndef MULIB_HEAPTABLE_H
#define MULIB_HEAPTABLE_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_heaptable
  {
    mulib_memtable_t rows;
    uint64_t last_id;
  } mulib_heaptable_t;
  /** @brief Zero init. Copy a row and return a never-reused positive ID; 0
   * success. */
  int mulib_heaptable_insert (mulib_heaptable_t *t, const void *data,
                              size_t size, uint64_t *id);
  /** @brief Borrow row until mutation/free; 0 found, 1 absent, negative
   * invalid. */
  int mulib_heaptable_get (const mulib_heaptable_t *t, uint64_t id,
                           const void **data, size_t *size);
  /** @brief Replace or tombstone an existing live row; 0 success, 1 absent. */
  int mulib_heaptable_update (mulib_heaptable_t *t, uint64_t id,
                              const void *data, size_t size, int deleted);
  /** @brief Release all rows/reset; IDs may be reused only after reset. */
  void mulib_heaptable_free (mulib_heaptable_t *t);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HEAPTABLE) && !defined(MULIB_HEAPTABLE_IMPLEMENTED)
#define MULIB_HEAPTABLE_IMPLEMENTED
static void
mulib_heaptable_key_ (uint64_t id, unsigned char k[8])
{
  unsigned i;
  for (i = 0; i < 8; ++i)
    k[i] = (unsigned char)(id >> (56 - 8 * i));
}
int
mulib_heaptable_insert (mulib_heaptable_t *t, const void *p, size_t n,
                        uint64_t *id)
{
  unsigned char k[8];
  int rc;
  if (!t || !id || t->last_id == UINT64_MAX)
    return -1;
  mulib_heaptable_key_ (t->last_id + 1, k);
  rc = mulib_memtable_put (&t->rows, k, 8, p, n, 0, 0);
  if (!rc)
    *id = ++t->last_id;
  return rc;
}
int
mulib_heaptable_get (const mulib_heaptable_t *t, uint64_t id, const void **p,
                     size_t *n)
{
  unsigned char k[8];
  const mulib_memtable_record_t *r;
  int rc;
  if (!t || !p || !n || !id)
    return -1;
  mulib_heaptable_key_ (id, k);
  rc = mulib_memtable_get (&t->rows, k, 8, &r);
  if (rc || r->deleted)
    return rc ? rc : 1;
  *p = r->value;
  *n = r->value_size;
  return 0;
}
int
mulib_heaptable_update (mulib_heaptable_t *t, uint64_t id, const void *p,
                        size_t n, int del)
{
  const void *old;
  size_t z;
  unsigned char k[8];
  int rc = mulib_heaptable_get (t, id, &old, &z);
  if (rc)
    return rc;
  mulib_heaptable_key_ (id, k);
  return mulib_memtable_put (&t->rows, k, 8, p, n, 0, del);
}
void
mulib_heaptable_free (mulib_heaptable_t *t)
{
  if (t)
    {
      mulib_memtable_free (&t->rows);
      t->last_id = 0;
    }
}
#endif
