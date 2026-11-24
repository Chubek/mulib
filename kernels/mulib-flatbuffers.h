/** @file mulib-flatbuffers.h
 * @brief Bounds-checked FlatBuffers table/scalar/string views, not a schema
 * compiler. Define MULIB_IMPL_FLATBUFFERS and MULIB_IMPL_BYCUTILS once.
 * Borrowed, immutable bytes. */
#ifndef MULIB_FLATBUFFERS_H
#define MULIB_FLATBUFFERS_H
#include "mulib-binfile.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_flatbuffers_table
  {
    mulib_binfile_t file;
    size_t table, vtable, object_size, vtable_size;
  } mulib_flatbuffers_table_t;
  /** @brief Open table at byte offset; signed vtable offsets may be
   * forward/backward. 1 success, 0 malformed/invalid. No whole-schema
   * verification is claimed. */
  int mulib_flatbuffers_table (const void *data, size_t size, size_t offset,
                               mulib_flatbuffers_table_t *out);
  /** @brief Open root from LE32 root offset; size-prefixed buffers not
   * supported. */
  int mulib_flatbuffers_root (const void *data, size_t size,
                              mulib_flatbuffers_table_t *out);
  /** @brief Scalar field id, width 1..8; 1 found, 0 absent/invalid. */
  int mulib_flatbuffers_uint (const mulib_flatbuffers_table_t *table,
                              unsigned field, unsigned width, uint64_t *out);
  /** @brief String field, checking forward offset, length and terminator. 1
   * found, 0 absent/malformed. UTF-8 is not checked; returned length excludes
   * terminator. */
  int mulib_flatbuffers_string (const mulib_flatbuffers_table_t *table,
                                unsigned field, mulib_binfile_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FLATBUFFERS) && !defined(MULIB_FLATBUFFERS_IMPLEMENTED)
#define MULIB_FLATBUFFERS_IMPLEMENTED
#include "mulib-bycutils.h"
int
mulib_flatbuffers_table (const void *data, size_t n, size_t o,
                         mulib_flatbuffers_table_t *out)
{
  uint64_t raw, vs, os;
  size_t vt;
  mulib_flatbuffers_table_t t;
  if (!out || !mulib_bycutils_load (data, n, o, 4, &raw) || !raw)
    return 0;
  if (raw <= INT32_MAX)
    {
      if (raw > o)
        return 0;
      vt = o - (size_t)raw;
    }
  else
    {
      uint64_t mag = UINT64_C (4294967296) - raw;
      if (mag > n - o)
        return 0;
      vt = o + (size_t)mag;
    }
  if (!mulib_bycutils_load (data, n, vt, 2, &vs) || vt > SIZE_MAX - 2
      || !mulib_bycutils_load (data, n, vt + 2, 2, &os) || vs < 4 || (vs & 1)
      || os < 4 || !mulib_bycutils_range (n, vt, (size_t)vs)
      || !mulib_bycutils_range (n, o, (size_t)os)
      || (vt < o + (size_t)os && o < vt + (size_t)vs))
    return 0;
  t.file.data = (const unsigned char *)data;
  t.file.size = n;
  t.table = o;
  t.vtable = vt;
  t.object_size = (size_t)os;
  t.vtable_size = (size_t)vs;
  *out = t;
  return 1;
}
int
mulib_flatbuffers_root (const void *p, size_t n,
                        mulib_flatbuffers_table_t *out)
{
  uint64_t o;
  return mulib_bycutils_load (p, n, 0, 4, &o) && o >= 4 && o <= SIZE_MAX
         && mulib_flatbuffers_table (p, n, (size_t)o, out);
}
static int
mulib_flatbuffers_field_ (const mulib_flatbuffers_table_t *t, unsigned f,
                          unsigned w, size_t *out)
{
  uint64_t off;
  size_t pos;
  if (!t || !w || w > 8 || f > 32765 || t->vtable_size < 4
      || (t->vtable_size & 1) || t->object_size < 4
      || !mulib_bycutils_range (t->file.size, t->vtable, t->vtable_size)
      || !mulib_bycutils_range (t->file.size, t->table, t->object_size))
    return 0;
  pos = 4 + 2 * (size_t)f;
  if (!mulib_bycutils_range (t->vtable_size, pos, 2)
      || !mulib_bycutils_load (t->file.data, t->file.size, t->vtable + pos, 2,
                               &off)
      || off < 4 || !mulib_bycutils_range (t->object_size, (size_t)off, w))
    return 0;
  *out = t->table + (size_t)off;
  return 1;
}
int
mulib_flatbuffers_uint (const mulib_flatbuffers_table_t *t, unsigned f,
                        unsigned w, uint64_t *out)
{
  size_t o;
  return mulib_flatbuffers_field_ (t, f, w, &o)
         && mulib_bycutils_load (t->file.data, t->file.size, o, w, out);
}
int
mulib_flatbuffers_string (const mulib_flatbuffers_table_t *t, unsigned f,
                          mulib_binfile_t *out)
{
  size_t o, start;
  uint64_t rel, n;
  if (!out || !mulib_flatbuffers_field_ (t, f, 4, &o)
      || !mulib_bycutils_load (t->file.data, t->file.size, o, 4, &rel)
      || rel < 4 || rel > t->file.size - o)
    return 0;
  start = o + (size_t)rel;
  if (!mulib_bycutils_load (t->file.data, t->file.size, start, 4, &n))
    return 0;
  start += 4;
  if (n >= t->file.size - start || t->file.data[start + (size_t)n])
    return 0;
  out->data = t->file.data + start;
  out->size = (size_t)n;
  return 1;
}
#endif
