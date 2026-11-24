/**
 * @file mulib-lsmtree.h
 * @brief Memory-backed LSM runs with WAL-before-publication and atomic
 * compaction. Define MULIB_IMPL_LSMTREE and all MEMTABLE/SSTFILE/WAL
 * dependencies explicitly. No disk durability, concurrent access or automatic
 * background work is implied.
 */
#ifndef MULIB_LSMTREE_H
#define MULIB_LSMTREE_H
#include "mulib-sstfile.h"
#include "mulib-wal.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Zero initialize; owns mutable table, immutable runs and WAL bytes.
   */
  typedef struct mulib_lsmtree
  {
    mulib_memtable_t memory;
    mulib_buffer_t *runs;
    size_t run_count;
    mulib_buffer_t wal;
    uint64_t sequence;
  } mulib_lsmtree_t;
  /** @brief Copy a put/delete (deleted=0/1). Failure leaves logical state and
   * WAL unchanged. */
  int mulib_lsmtree_put (mulib_lsmtree_t *t, const void *key, size_t kn,
                         const void *value, size_t vn, int deleted);
  /** @brief Copy value to buffer; 0 found, 1 absent/deleted, negative on
   * failure. */
  int mulib_lsmtree_get (const mulib_lsmtree_t *t, const void *key, size_t kn,
                         mulib_buffer_t *out);
  /** @brief Publish the mutable table as another immutable run; WAL is
   * retained. */
  int mulib_lsmtree_flush (mulib_lsmtree_t *t);
  /** @brief Merge runs atomically; retain tombstones and mutable records
   * separately. */
  int mulib_lsmtree_compact (mulib_lsmtree_t *t);
  /** @brief Replay a WAL into a zero/uninitialized tree. Accept a torn last
   * frame only when allow_torn=1; consumed reports the validated prefix.
   * Corruption fails. */
  int mulib_lsmtree_recover (const void *log, size_t size, int allow_torn,
                             mulib_lsmtree_t *out, size_t *consumed);
  /** @brief Free/reset all state; no implicit persistence. */
  void mulib_lsmtree_free (mulib_lsmtree_t *t);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LSMTREE) && !defined(MULIB_LSMTREE_IMPLEMENTED)
#define MULIB_LSMTREE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
void
mulib_lsmtree_free (mulib_lsmtree_t *t)
{
  size_t i;
  if (!t)
    return;
  mulib_memtable_free (&t->memory);
  mulib_buffer_free (&t->wal);
  for (i = 0; i < t->run_count; ++i)
    mulib_buffer_free (t->runs + i);
  free (t->runs);
  memset (t, 0, sizeof (*t));
}
int
mulib_lsmtree_put (mulib_lsmtree_t *t, const void *k, size_t kn, const void *v,
                   size_t vn, int del)
{
  mulib_memtable_t next = { 0 };
  int rc;
  if (!t || t->sequence == UINT64_MAX)
    return -1;
  rc = mulib_memtable_clone (&t->memory, &next);
  if (rc)
    return rc;
  rc = mulib_memtable_put (&next, k, kn, v, vn, t->sequence + 1, del);
  if (!rc)
    rc = mulib_wal_append (&t->wal, k, kn, v, vn, t->sequence + 1, del);
  if (rc)
    {
      mulib_memtable_free (&next);
      return rc;
    }
  mulib_memtable_free (&t->memory);
  t->memory = next;
  ++t->sequence;
  return 0;
}
int
mulib_lsmtree_get (const mulib_lsmtree_t *t, const void *k, size_t kn,
                   mulib_buffer_t *out)
{
  const mulib_memtable_record_t *r;
  size_t i;
  int rc;
  if (!t || !out || (!k && kn))
    return -1;
  rc = mulib_memtable_get (&t->memory, k, kn, &r);
  if (!rc)
    return r->deleted
               ? 1
               : (mulib_buffer_append (out, r->value, r->value_size) ? 0 : -2);
  for (i = t->run_count; i > 0; --i)
    {
      mulib_memtable_t run = { 0 };
      rc = mulib_sstfile_decode (t->runs[i - 1].data, t->runs[i - 1].length,
                                 &run);
      if (rc)
        return rc;
      rc = mulib_memtable_get (&run, k, kn, &r);
      if (!rc)
        rc = r->deleted
                 ? 1
                 : (mulib_buffer_append (out, r->value, r->value_size) ? 0
                                                                       : -2);
      else
        rc = 2;
      mulib_memtable_free (&run);
      if (rc != 2)
        return rc;
    }
  return 1;
}
int
mulib_lsmtree_flush (mulib_lsmtree_t *t)
{
  mulib_buffer_t b = { 0 }, *next;
  int rc;
  if (!t)
    return -1;
  if (!t->memory.count)
    return 0;
  if (t->run_count >= SIZE_MAX / sizeof (*next))
    return -1;
  rc = mulib_sstfile_encode (&t->memory, &b);
  if (rc)
    return rc;
  next = (mulib_buffer_t *)realloc (t->runs,
                                    (t->run_count + 1) * sizeof (*next));
  if (!next)
    {
      mulib_buffer_free (&b);
      return -2;
    }
  t->runs = next;
  next[t->run_count++] = b;
  mulib_memtable_free (&t->memory);
  return 0;
}
int
mulib_lsmtree_compact (mulib_lsmtree_t *t)
{
  mulib_memtable_t all = { 0 };
  mulib_buffer_t b = { 0 };
  size_t i, j;
  int rc = 0;
  if (!t)
    return -1;
  if (t->run_count < 2)
    return 0;
  for (i = 0; i < t->run_count && !rc; ++i)
    {
      mulib_memtable_t run = { 0 };
      rc = mulib_sstfile_decode (t->runs[i].data, t->runs[i].length, &run);
      for (j = 0; j < run.count && !rc; ++j)
        {
          const mulib_memtable_record_t *r = run.records + j;
          rc = mulib_memtable_put (&all, r->key, r->key_size, r->value,
                                   r->value_size, r->sequence, r->deleted);
        }
      mulib_memtable_free (&run);
    }
  if (!rc)
    rc = mulib_sstfile_encode (&all, &b);
  mulib_memtable_free (&all);
  if (rc)
    {
      mulib_buffer_free (&b);
      return rc;
    }
  for (i = 0; i < t->run_count; ++i)
    mulib_buffer_free (t->runs + i);
  t->runs[0] = b;
  t->run_count = 1;
  return 0;
}
int
mulib_lsmtree_recover (const void *p, size_t n, int torn, mulib_lsmtree_t *out,
                       size_t *consumed)
{
  mulib_lsmtree_t t = { 0 };
  mulib_wal_reader_t r = { 0 };
  mulib_wal_record_t v;
  int rc;
  if (!out || (!p && n) || (torn != 0 && torn != 1))
    return -1;
  r.data = (const unsigned char *)p;
  r.size = n;
  while ((rc = mulib_wal_next (&r, &v)) == MULIB_WAL_RECORD)
    {
      rc = mulib_memtable_put (&t.memory, v.key, v.key_size, v.value,
                               v.value_size, v.sequence, v.deleted);
      if (rc)
        goto fail;
      t.sequence = v.sequence;
    }
  if (rc && !(rc == MULIB_WAL_TORN && torn))
    goto fail;
  if (!mulib_buffer_append (&t.wal, p, r.offset))
    {
      rc = -2;
      goto fail;
    }
  *out = t;
  if (consumed)
    *consumed = r.offset;
  return 0;
fail:
  mulib_lsmtree_free (&t);
  return rc > 0 ? -2 : rc;
}
#endif
