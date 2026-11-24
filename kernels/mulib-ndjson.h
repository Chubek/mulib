/* MuLIB: Bounded borrowed line iteration over newline-delimited JSON source bytes. C11
 * single-header; define MULIB_IMPL_NDJSON once. */
#ifndef MULIB_NDJSON_H
#define MULIB_NDJSON_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    const char *data;
    size_t length, offset, line;
  } mulib_ndjson_t;
  void mulib_ndjson_init (mulib_ndjson_t *p, const char *data, size_t length);
  /* 1 line, 0 eof, -1 invalid; excludes trailing CR/LF, skips blank lines. */
  int mulib_ndjson_next (mulib_ndjson_t *p, const char **line, size_t *length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NDJSON) && !defined(MULIB_NDJSON_IMPLEMENTED)
#define MULIB_NDJSON_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  void
  mulib_ndjson_init (mulib_ndjson_t *p, const char *s, size_t n)
  {
    if (!p)
      return;
    p->data = s;
    p->length = s ? n : 0;
    p->offset = 0;
    p->line = 0;
  }
  int
  mulib_ndjson_next (mulib_ndjson_t *p, const char **out, size_t *len)
  {
    if (!p || !out || !len || (!p->data && p->length))
      return -1;
    while (p->offset < p->length)
      {
        size_t start = p->offset, i = start;
        while (i < p->length && p->data[i] != '\n')
          i++;
        p->offset = i + (i < p->length);
        p->line++;
        size_t n = i - start;
        if (n && p->data[start + n - 1] == '\r')
          n--;
        if (!n)
          continue;
        *out = p->data + start;
        *len = n;
        return 1;
      }
    return 0;
  }
#ifdef __cplusplus
}
#endif
#endif
