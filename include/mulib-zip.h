/** @file mulib-zip.h
 * @brief Checked memory ZIP32 archives with stored and DEFLATE entries.
 * Define MULIB_IMPL_ZIP, DEFLATE, BUFFER, CRC32 and UTF8. No filesystem I/O.
 */
#ifndef MULIB_ZIP_H
#define MULIB_ZIP_H
#include "mulib-crc32.h"
#include "mulib-deflate.h"
#include "mulib-utf8.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Zero is success; negative values distinguish failures. */
  enum mulib_zip_result
  {
    MULIB_ZIP_OK = 0,
    MULIB_ZIP_EINVAL = -1,
    MULIB_ZIP_EFORMAT = -2,
    MULIB_ZIP_EUNSUPPORTED = -3,
    MULIB_ZIP_ENOMEM = -4,
    MULIB_ZIP_ELIMIT = -5,
    MULIB_ZIP_ECRC = -6,
    MULIB_ZIP_ENOTFOUND = -7
  };
  /** @brief Borrowed, non-terminated name; offsets are into the original
   * archive. */
  typedef struct mulib_zip_entry
  {
    const unsigned char *name;
    size_t name_length, offset, data_offset, end_offset;
    uint32_t size, compressed_size, crc;
    unsigned method, flags;
  } mulib_zip_entry_t;
  /** @brief Zero initialize; owns entries, borrows immutable archive bytes. */
  typedef struct mulib_zip_reader
  {
    const unsigned char *data;
    size_t size, count;
    mulib_zip_entry_t *entries;
  } mulib_zip_reader_t;
  /** @brief Zero initialize; owns both buffers. Do not copy live writers. */
  typedef struct mulib_zip_writer
  {
    mulib_buffer_t bytes, directory;
    size_t count;
    int finished;
  } mulib_zip_writer_t;
  /** @brief Portable UTF-8 relative file/directory path check. */
  int mulib_zip_safe_path (const char *path);
  /** @brief Validate ZIP32 structure and index entries in local-file order.
   * Reader must be zero initialized. Failure leaves it unchanged. */
  int mulib_zip_open (mulib_zip_reader_t *reader, const void *data,
                      size_t size);
  /** @brief Release index and zero reader; does not release input. */
  void mulib_zip_close (mulib_zip_reader_t *reader);
  /** @brief Borrow entry by exact path, or NULL. */
  const mulib_zip_entry_t *mulib_zip_find (const mulib_zip_reader_t *reader,
                                           const char *path);
  /** @brief Decode indexed entry and verify CRC, appending atomically. limit
   * is the maximum uncompressed entry size; output may alias archive storage.
   */
  int mulib_zip_extract (const mulib_zip_reader_t *reader, size_t index,
                         size_t limit, mulib_buffer_t *out);
  /** @brief Copy a unique path/payload into writer, method 0 (stored) or 8
   * (DEFLATE). Inputs must not alias writer storage. Failure is
   * logical-atomic. */
  int mulib_zip_add (mulib_zip_writer_t *writer, const char *path,
                     const void *data, size_t size, unsigned method);
  /** @brief Append directory and EOCD. Idempotent; freezes additions. */
  int mulib_zip_finish (mulib_zip_writer_t *writer);
  /** @brief Free/reset writer. NULL is harmless. */
  void mulib_zip_writer_free (mulib_zip_writer_t *writer);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ZIP) && !defined(MULIB_ZIP_IMPLEMENTED)
#define MULIB_ZIP_IMPLEMENTED
static unsigned
mulib_zip_u16_ (const unsigned char *p)
{
  return p[0] | ((unsigned)p[1] << 8);
}
static uint32_t
mulib_zip_u32_ (const unsigned char *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}
static void
mulib_zip_put16_ (unsigned char *p, unsigned v)
{
  p[0] = (unsigned char)v;
  p[1] = (unsigned char)(v >> 8);
}
static void
mulib_zip_put32_ (unsigned char *p, uint32_t v)
{
  p[0] = (unsigned char)v;
  p[1] = (unsigned char)(v >> 8);
  p[2] = (unsigned char)(v >> 16);
  p[3] = (unsigned char)(v >> 24);
}
static int
mulib_zip_path_ (const unsigned char *p, size_t n)
{
  size_t i, start = 0;
  if (!n || n > 65535 || p[0] == '/' || !mulib_utf8_validate (p, n))
    return 0;
  for (i = 0; i <= n; ++i)
    {
      if (i < n
          && (p[i] < 32 || p[i] == 127 || p[i] == '\\' || p[i] == ':'
              || p[i] == 0))
        return 0;
      if (i == n || p[i] == '/')
        {
          size_t len = i - start;
          if (!len)
            return i == n && i > 0 && p[i - 1] == '/';
          if ((len == 1 && p[start] == '.')
              || (len == 2 && p[start] == '.' && p[start + 1] == '.'))
            return 0;
          start = i + 1;
        }
    }
  return 1;
}
int
mulib_zip_safe_path (const char *path)
{
  return path && mulib_zip_path_ ((const unsigned char *)path, strlen (path));
}
static int
mulib_zip_namecmp_ (const void *a, const void *b)
{
  const mulib_zip_entry_t *x = (const mulib_zip_entry_t *)a,
                          *y = (const mulib_zip_entry_t *)b;
  size_t n = x->name_length < y->name_length ? x->name_length : y->name_length;
  int c = memcmp (x->name, y->name, n);
  return c                                 ? c
         : x->name_length < y->name_length ? -1
                                           : x->name_length > y->name_length;
}
static int
mulib_zip_offsetcmp_ (const void *a, const void *b)
{
  size_t x = ((const mulib_zip_entry_t *)a)->offset,
         y = ((const mulib_zip_entry_t *)b)->offset;
  return x < y ? -1 : x > y;
}
static int
mulib_zip_extra_ (const unsigned char *p, size_t n)
{
  while (n)
    {
      unsigned id, len;
      if (n < 4)
        return MULIB_ZIP_EFORMAT;
      id = mulib_zip_u16_ (p);
      len = mulib_zip_u16_ (p + 2);
      if (len > n - 4)
        return MULIB_ZIP_EFORMAT;
      if (id == 1)
        return MULIB_ZIP_EUNSUPPORTED;
      p += 4 + len;
      n -= 4 + len;
    }
  return 0;
}
int
mulib_zip_open (mulib_zip_reader_t *r, const void *data, size_t size)
{
  const unsigned char *p = (const unsigned char *)data, *c, *h;
  mulib_zip_reader_t tmp;
  size_t end, low, cd, cds, pos, i, n, extra, comment, off, payload;
  int result = MULIB_ZIP_EFORMAT;
  if (!r || r->entries || r->data || (!data && size))
    return MULIB_ZIP_EINVAL;
  if (size < 22)
    return result;
  memset (&tmp, 0, sizeof (tmp));
  end = size - 22;
  low = end > 65535 ? end - 65535 : 0;
  for (;;)
    {
      if (mulib_zip_u32_ (p + end) == UINT32_C (0x06054b50)
          && mulib_zip_u16_ (p + end + 20) == size - end - 22)
        {
          size_t start = mulib_zip_u32_ (p + end + 16);
          size_t length = mulib_zip_u32_ (p + end + 12);
          size_t count = mulib_zip_u16_ (p + end + 10);
          if (start == UINT32_MAX || length == UINT32_MAX || count == 65535)
            result = MULIB_ZIP_EUNSUPPORTED;
          /* Signatures can occur inside a ZIP comment. Do not accept one as
           * EOCD unless its central-directory geometry is also possible. */
          else if (start <= end && length == end - start
                   && count <= length / 46
                   && (count ? mulib_zip_u32_ (p + start)
                                   == UINT32_C (0x02014b50)
                             : length == 0))
            break;
        }
      if (end == low)
        return result;
      --end;
    }
  result = MULIB_ZIP_EFORMAT;
  if (mulib_zip_u16_ (p + end + 4) || mulib_zip_u16_ (p + end + 6)
      || mulib_zip_u16_ (p + end + 8) != mulib_zip_u16_ (p + end + 10))
    return MULIB_ZIP_EUNSUPPORTED;
  tmp.count = mulib_zip_u16_ (p + end + 10);
  cd = mulib_zip_u32_ (p + end + 16);
  cds = mulib_zip_u32_ (p + end + 12);
  if (tmp.count == 65535 || cd == UINT32_MAX || cds == UINT32_MAX)
    return MULIB_ZIP_EUNSUPPORTED;
  if (cd > end || cds != end - cd || tmp.count > cds / 46)
    return result;
  tmp.entries = (mulib_zip_entry_t *)calloc (tmp.count ? tmp.count : 1,
                                             sizeof (*tmp.entries));
  if (!tmp.entries)
    return MULIB_ZIP_ENOMEM;
  pos = cd;
  for (i = 0; i < tmp.count; ++i)
    {
      mulib_zip_entry_t *e = &tmp.entries[i];
      if (end - pos < 46 || mulib_zip_u32_ (p + pos) != UINT32_C (0x02014b50))
        goto fail;
      c = p + pos;
      n = mulib_zip_u16_ (c + 28);
      extra = mulib_zip_u16_ (c + 30);
      comment = mulib_zip_u16_ (c + 32);
      if (n + extra + comment > end - pos - 46)
        goto fail;
      e->name = c + 46;
      e->name_length = n;
      if (!mulib_zip_path_ (e->name, n))
        goto fail;
      e->flags = mulib_zip_u16_ (c + 8);
      e->method = mulib_zip_u16_ (c + 10);
      if ((e->flags & ~0x080eu) || (e->method != 0 && e->method != 8)
          || (e->method == 0 && (e->flags & 6)) || mulib_zip_u16_ (c + 34)
          || mulib_zip_u16_ (c + 6) > 20)
        {
          result = MULIB_ZIP_EUNSUPPORTED;
          goto fail;
        }
      e->crc = mulib_zip_u32_ (c + 16);
      e->compressed_size = mulib_zip_u32_ (c + 20);
      e->size = mulib_zip_u32_ (c + 24);
      off = mulib_zip_u32_ (c + 42);
      e->offset = off;
      if (e->size == UINT32_MAX || e->compressed_size == UINT32_MAX
          || off == UINT32_MAX)
        {
          result = MULIB_ZIP_EUNSUPPORTED;
          goto fail;
        }
      result = mulib_zip_extra_ (c + 46 + n, extra);
      if (result)
        goto fail;
      result = MULIB_ZIP_EFORMAT;
      if (mulib_zip_u16_ (c + 6)
          < (e->method == 8 || (e->flags & 8) ? 20 : 10))
        goto fail;
      if (off > cd || cd - off < 30)
        goto fail;
      h = p + off;
      if (mulib_zip_u32_ (h) != UINT32_C (0x04034b50)
          || mulib_zip_u16_ (h + 6) != e->flags
          || mulib_zip_u16_ (h + 8) != e->method
          || mulib_zip_u16_ (h + 26) != n
          || mulib_zip_u16_ (h + 4) != mulib_zip_u16_ (c + 6)
          || memcmp (h + 10, c + 12, 4))
        goto fail;
      extra = mulib_zip_u16_ (h + 28);
      if (n + extra > cd - off - 30 || memcmp (h + 30, e->name, n))
        goto fail;
      result = mulib_zip_extra_ (h + 30 + n, extra);
      if (result)
        goto fail;
      result = MULIB_ZIP_EFORMAT;
      payload = off + 30 + n + extra;
      e->data_offset = payload;
      if (e->compressed_size > cd - payload)
        goto fail;
      e->end_offset = payload + e->compressed_size;
      if (e->method == 0 && e->size != e->compressed_size)
        goto fail;
      if (!(e->flags & 8))
        {
          if (mulib_zip_u32_ (h + 14) != e->crc
              || mulib_zip_u32_ (h + 18) != e->compressed_size
              || mulib_zip_u32_ (h + 22) != e->size)
            goto fail;
        }
      else
        {
          size_t tail = e->end_offset;
          if ((mulib_zip_u32_ (h + 14) && mulib_zip_u32_ (h + 14) != e->crc)
              || (mulib_zip_u32_ (h + 18)
                  && mulib_zip_u32_ (h + 18) != e->compressed_size)
              || (mulib_zip_u32_ (h + 22)
                  && mulib_zip_u32_ (h + 22) != e->size))
            goto fail;
          if (cd - tail < 12)
            goto fail;
          /* A signature-less descriptor's CRC can itself equal the optional
           * signature. Prefer a matching bare descriptor before skipping it.
           */
          if (!(mulib_zip_u32_ (p + tail) == e->crc
                && mulib_zip_u32_ (p + tail + 4) == e->compressed_size
                && mulib_zip_u32_ (p + tail + 8) == e->size))
            {
              if (mulib_zip_u32_ (p + tail) != UINT32_C (0x08074b50))
                goto fail;
              tail += 4;
            }
          if (cd - tail < 12 || mulib_zip_u32_ (p + tail) != e->crc
              || mulib_zip_u32_ (p + tail + 4) != e->compressed_size
              || mulib_zip_u32_ (p + tail + 8) != e->size)
            goto fail;
          e->end_offset = tail + 12;
        }
      if (e->name[n - 1] == '/' && e->size)
        goto fail;
      pos += 46 + n + mulib_zip_u16_ (c + 30) + comment;
    }
  if (pos != end)
    goto fail;
  qsort (tmp.entries, tmp.count, sizeof (*tmp.entries), mulib_zip_namecmp_);
  for (i = 1; i < tmp.count; ++i)
    if (!mulib_zip_namecmp_ (&tmp.entries[i - 1], &tmp.entries[i]))
      goto fail;
  qsort (tmp.entries, tmp.count, sizeof (*tmp.entries), mulib_zip_offsetcmp_);
  for (i = 1; i < tmp.count; ++i)
    if (tmp.entries[i].offset < tmp.entries[i - 1].end_offset)
      goto fail;
  tmp.data = p;
  tmp.size = size;
  *r = tmp;
  return 0;
fail:
  free (tmp.entries);
  return result;
}
void
mulib_zip_close (mulib_zip_reader_t *r)
{
  if (r)
    {
      free (r->entries);
      memset (r, 0, sizeof (*r));
    }
}
const mulib_zip_entry_t *
mulib_zip_find (const mulib_zip_reader_t *r, const char *path)
{
  size_t i, n;
  if (!r || !path)
    return NULL;
  n = strlen (path);
  for (i = 0; i < r->count; ++i)
    if (r->entries[i].name_length == n
        && !memcmp (r->entries[i].name, path, n))
      return &r->entries[i];
  return NULL;
}
int
mulib_zip_extract (const mulib_zip_reader_t *r, size_t index, size_t limit,
                   mulib_buffer_t *out)
{
  const mulib_zip_entry_t *e;
  mulib_buffer_t tmp = { 0 };
  int result;
  if (!r || !r->data || !out || index >= r->count)
    return MULIB_ZIP_EINVAL;
  e = &r->entries[index];
  if (e->size > limit)
    return MULIB_ZIP_ELIMIT;
  if (e->method == 0)
    result = mulib_buffer_append (&tmp, r->data + e->data_offset, e->size)
                 ? 0
                 : MULIB_ZIP_ENOMEM;
  else
    {
      int rc = mulib_deflate_decode (r->data + e->data_offset,
                                     e->compressed_size, e->size, &tmp);
      result = rc == MULIB_DEFLATE_ENOMEM ? MULIB_ZIP_ENOMEM
               : rc                       ? MULIB_ZIP_EFORMAT
                                          : 0;
    }
  if (!result && tmp.length != e->size)
    result = MULIB_ZIP_EFORMAT;
  if (!result && mulib_crc32 (tmp.data, tmp.length) != e->crc)
    result = MULIB_ZIP_ECRC;
  if (!result && !mulib_buffer_append (out, tmp.data, tmp.length))
    result = MULIB_ZIP_ENOMEM;
  mulib_buffer_free (&tmp);
  return result;
}
int
mulib_zip_add (mulib_zip_writer_t *w, const char *path, const void *data,
               size_t size, unsigned method)
{
  mulib_buffer_t compressed = { 0 }, local = { 0 }, central = { 0 };
  unsigned char h[46] = { 0 };
  size_t n, pos, old;
  uint32_t crc;
  int result = MULIB_ZIP_ENOMEM;
  if (!w || w->finished || !mulib_zip_safe_path (path) || (!data && size))
    return MULIB_ZIP_EINVAL;
  if (method != 0 && method != 8)
    return MULIB_ZIP_EUNSUPPORTED;
  n = strlen (path);
  if (path[n - 1] == '/' && size)
    return MULIB_ZIP_EINVAL;
  if (size >= UINT32_MAX || w->bytes.length >= UINT32_MAX || w->count >= 65534)
    return MULIB_ZIP_ELIMIT;
  for (pos = 0; pos < w->directory.length;)
    {
      const unsigned char *c = w->directory.data + pos;
      size_t len = mulib_zip_u16_ (c + 28);
      if (len == n && !memcmp (c + 46, path, n))
        return MULIB_ZIP_EINVAL;
      pos += 46 + len;
    }
  if (method == 8)
    {
      if (mulib_deflate_encode (data, size, &compressed))
        goto done;
    }
  else if (!mulib_buffer_append (&compressed, data, size))
    goto done;
  if (compressed.length >= UINT32_MAX || n + 30 > UINT32_MAX - w->bytes.length
      || compressed.length >= UINT32_MAX - w->bytes.length - n - 30)
    {
      result = MULIB_ZIP_ELIMIT;
      goto done;
    }
  crc = mulib_crc32 (data, size);
  mulib_zip_put32_ (h, UINT32_C (0x04034b50));
  mulib_zip_put16_ (h + 4, 20);
  mulib_zip_put16_ (h + 6, 0x800);
  mulib_zip_put16_ (h + 8, method);
  mulib_zip_put16_ (h + 12, 33);
  mulib_zip_put32_ (h + 14, crc);
  mulib_zip_put32_ (h + 18, (uint32_t)compressed.length);
  mulib_zip_put32_ (h + 22, (uint32_t)size);
  mulib_zip_put16_ (h + 26, (unsigned)n);
  if (!mulib_buffer_append (&local, h, 30)
      || !mulib_buffer_append (&local, path, n)
      || !mulib_buffer_append (&local, compressed.data, compressed.length))
    goto done;
  memset (h, 0, sizeof (h));
  mulib_zip_put32_ (h, UINT32_C (0x02014b50));
  mulib_zip_put16_ (h + 4, 20);
  mulib_zip_put16_ (h + 6, 20);
  mulib_zip_put16_ (h + 8, 0x800);
  mulib_zip_put16_ (h + 10, method);
  mulib_zip_put16_ (h + 14, 33);
  mulib_zip_put32_ (h + 16, crc);
  mulib_zip_put32_ (h + 20, (uint32_t)compressed.length);
  mulib_zip_put32_ (h + 24, (uint32_t)size);
  mulib_zip_put16_ (h + 28, (unsigned)n);
  mulib_zip_put32_ (h + 42, (uint32_t)w->bytes.length);
  if (!mulib_buffer_append (&central, h, 46)
      || !mulib_buffer_append (&central, path, n))
    goto done;
  old = w->bytes.length;
  if (!mulib_buffer_append (&w->bytes, local.data, local.length))
    goto done;
  if (!mulib_buffer_append (&w->directory, central.data, central.length))
    {
      w->bytes.length = old;
      goto done;
    }
  ++w->count;
  result = 0;
done:
  mulib_buffer_free (&compressed);
  mulib_buffer_free (&local);
  mulib_buffer_free (&central);
  return result;
}
int
mulib_zip_finish (mulib_zip_writer_t *w)
{
  mulib_buffer_t tail = { 0 };
  unsigned char h[22] = { 0 };
  int ok;
  if (!w)
    return MULIB_ZIP_EINVAL;
  if (w->finished)
    return 0;
  if (w->bytes.length > UINT32_MAX - 22
      || w->directory.length > UINT32_MAX - 22 - w->bytes.length)
    return MULIB_ZIP_ELIMIT;
  mulib_zip_put32_ (h, UINT32_C (0x06054b50));
  mulib_zip_put16_ (h + 8, (unsigned)w->count);
  mulib_zip_put16_ (h + 10, (unsigned)w->count);
  mulib_zip_put32_ (h + 12, (uint32_t)w->directory.length);
  mulib_zip_put32_ (h + 16, (uint32_t)w->bytes.length);
  ok = mulib_buffer_append (&tail, w->directory.data, w->directory.length)
       && mulib_buffer_append (&tail, h, 22)
       && mulib_buffer_append (&w->bytes, tail.data, tail.length);
  mulib_buffer_free (&tail);
  if (!ok)
    return MULIB_ZIP_ENOMEM;
  w->finished = 1;
  return 0;
}
void
mulib_zip_writer_free (mulib_zip_writer_t *w)
{
  if (w)
    {
      mulib_buffer_free (&w->bytes);
      mulib_buffer_free (&w->directory);
      memset (w, 0, sizeof (*w));
    }
}
#endif
