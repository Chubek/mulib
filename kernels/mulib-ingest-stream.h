/* MuLIB: Bounded chunk accumulation and newline record streaming with callback. C11
 * single-header; define MULIB_IMPL_INGEST_STREAM once. */
#ifndef MULIB_INGEST_STREAM_H
#define MULIB_INGEST_STREAM_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    unsigned char *data;
    size_t length, capacity, max_record;
  } mulib_ingest_stream_t;
  typedef int (*mulib_ingest_record_fn) (const void *bytes, size_t length, void *ctx);
  int mulib_ingest_stream_init (mulib_ingest_stream_t *s, size_t max_record);
  /* Return processed record count, SIZE_MAX on invalid/callback failure. */
  size_t mulib_ingest_stream_push (mulib_ingest_stream_t *s, const void *data,
                                   size_t length, mulib_ingest_record_fn fn, void *ctx);
  /* Emit a nonempty final unterminated record; 1 emitted, 0 empty, SIZE_MAX error.
   * A rejected record stays buffered for retry. Initialize from zero, free to reuse. */
  size_t mulib_ingest_stream_finish (mulib_ingest_stream_t *s,
                                     mulib_ingest_record_fn fn, void *ctx);
  void mulib_ingest_stream_free (mulib_ingest_stream_t *s);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_INGEST_STREAM) && !defined(MULIB_INGEST_STREAM_IMPLEMENTED)
#define MULIB_INGEST_STREAM_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_ingest_stream_init (mulib_ingest_stream_t *s, size_t max)
  {
    if (!s || s->data || s->max_record || !max || max == SIZE_MAX)
      return 0;
    memset (s, 0, sizeof (*s));
    s->max_record = max;
    return 1;
  }
  size_t
  mulib_ingest_stream_push (mulib_ingest_stream_t *s, const void *p, size_t n,
                            mulib_ingest_record_fn fn, void *ctx)
  {
    if (!s || !s->max_record || (!p && n) || !fn)
      return SIZE_MAX;
    const unsigned char *bytes = (const unsigned char *)p;
    size_t count = 0;
    for (size_t i = 0; i < n; i++)
      {
        unsigned char c = bytes[i];
        if (c == '\n')
          {
            size_t length = s->length;
            if (length && s->data[length - 1] == '\r')
              length--;
            if (!fn (s->data, length, ctx))
              return SIZE_MAX;
            count++;
            s->length = 0;
            continue;
          }
        if (s->length >= s->max_record)
          return SIZE_MAX;
        if (s->length == s->capacity)
          {
            size_t cap = s->capacity ? s->capacity * 2 : 128;
            if (cap < s->capacity)
              return SIZE_MAX;
            if (cap > s->max_record)
              cap = s->max_record;
            void *tmp = realloc (s->data, cap);
            if (!tmp)
              return SIZE_MAX;
            s->data = (unsigned char *)tmp;
            s->capacity = cap;
          }
        s->data[s->length++] = c;
      }
    return count;
  }
  void
  mulib_ingest_stream_free (mulib_ingest_stream_t *s)
  {
    if (!s)
      return;
    free (s->data);
    memset (s, 0, sizeof (*s));
  }
  size_t
  mulib_ingest_stream_finish (mulib_ingest_stream_t *s, mulib_ingest_record_fn fn,
                              void *ctx)
  {
    if (!s || !s->max_record || !fn)
      return SIZE_MAX;
    if (!s->length)
      return 0;
    size_t n = s->length;
    if (s->data[n - 1] == '\r')
      n--;
    if (!fn (s->data, n, ctx))
      return SIZE_MAX;
    s->length = 0;
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
