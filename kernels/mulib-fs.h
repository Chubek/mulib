/**
 * @file mulib-fs.h
 * @brief Whole-file read and write helpers with explicit byte length.
 * @defgroup mulib_fs MuLIB fs
 * @{
 * @details Reads add a convenience NUL byte; writes truncate existing files
 * and are not atomic. Paths are not confined.
 * @par Integration
 * Define MULIB_IMPL_FS in exactly one translation unit. ISO C11, C++17
 * declarations with C linkage.
 * @}
 */
#ifndef MULIB_FS_H
#define MULIB_FS_H
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** Caller frees result. On failure leaves *length unchanged. */
  unsigned char *mulib_fs_read (const char *path, size_t *length);
  /** Return NULL rather than truncate when the file exceeds limit. Caller frees
   * the result. On failure sets *length to zero, if supplied. */
  unsigned char *mulib_fs_read_bounded (const char *path, size_t limit,
                                       size_t *length);
  /** Writes with fopen("wb"): not atomic, path is not confined. */
  int mulib_fs_write (const char *path, const void *data, size_t length);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_FS_H */
#if defined(MULIB_IMPL_FS) && !defined(MULIB_FS_IMPLEMENTED)
#define MULIB_FS_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  unsigned char *
  mulib_fs_read_bounded (const char *path, size_t limit, size_t *length)
  {
    if (length)
      *length = 0;
    if (!path)
      {
#ifdef EINVAL
        errno = EINVAL;
#endif
        return NULL;
      }
    if (limit == SIZE_MAX)
      limit = SIZE_MAX - 1; /* reserve space for the NUL */
    FILE *f = fopen (path, "rb");
    if (!f)
      return NULL;
    size_t cap = limit < 4096 ? limit : 4096, n = 0;
    unsigned char *b = malloc (cap + 1);
    if (!b)
      {
        fclose (f);
        return NULL;
      }
    int ok = 1;
    for (;;)
      {
        if (n == cap)
          {
            if (cap == limit)
              {
                int ch = fgetc (f);
                if (ch != EOF)
                  {
#ifdef EFBIG
                    errno = EFBIG;
#endif
                    ok = 0;
                  }
                else if (ferror (f))
                  ok = 0;
                break;
              }
            size_t next = cap > limit / 2 ? limit : cap * 2;
            unsigned char *p = realloc (b, next + 1);
            if (!p)
              {
                ok = 0;
                break;
              }
            b = p;
            cap = next;
          }
        size_t got = fread (b + n, 1, cap - n, f);
        n += got;
        if (ferror (f))
          {
            ok = 0;
            break;
          }
        if (feof (f))
          break;
      }
    int saved = errno;
    if (fclose (f) != 0 && ok)
      {
        saved = errno;
        ok = 0;
      }
    if (!ok)
      {
        free (b);
        errno = saved;
        return NULL;
      }
    b[n] = 0;
    if (length)
      *length = n;
    return b;
  }
  unsigned char *
  mulib_fs_read (const char *path, size_t *length)
  {
    size_t size;
    unsigned char *data = mulib_fs_read_bounded (path, SIZE_MAX, &size);
    if (data && length)
      *length = size;
    return data;
  }
  int
  mulib_fs_write (const char *path, const void *p, size_t n)
  {
    if (!path || (!p && n))
      return 0;
    FILE *f = fopen (path, "wb");
    if (!f)
      return 0;
    size_t done = n ? fwrite (p, 1, n, f) : 0;
    int ok = done == n && fflush (f) == 0;
    return fclose (f) == 0 && ok;
  }
#ifdef __cplusplus
}
#endif /* implementation */
#endif
