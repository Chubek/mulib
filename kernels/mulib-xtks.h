/** @file mulib-xtks.h
 * @brief X transport toolkit: checked borrowed streams and resumable exact
 * I/O. Define MULIB_IMPL_XTKS once. No allocation or implicit descriptor
 * ownership.
 */
#ifndef MULIB_XTKS_H
#define MULIB_XTKS_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_XTKS_OK = 0,
    MULIB_XTKS_AGAIN = 1,
    MULIB_XTKS_EOF = 2,
    MULIB_XTKS_EINVAL = -1,
    MULIB_XTKS_EIO = -2
  };
  /** @brief Callbacks return OK with positive progress, AGAIN/EOF with zero
   * count, or a negative error. Zero-length calls are handled by the toolkit.
   * EOF is only valid for reads. The context and operations are borrowed. */
  typedef struct mulib_xtks
  {
    void *context;
    int (*read) (void *, void *, size_t, size_t *);
    int (*write) (void *, const void *, size_t, size_t *);
  } mulib_xtks_t;
  /** @brief Checked single read/write. Count is reset on failure. */
  int mulib_xtks_read (const mulib_xtks_t *, void *, size_t, size_t *);
  int mulib_xtks_write (const mulib_xtks_t *, const void *, size_t, size_t *);
  /** @brief Transfer exactly length bytes, resuming at *offset (initially
   * zero). Preserve offset and buffer across AGAIN. EOF may leave a partial
   * read. */
  int mulib_xtks_read_exact (const mulib_xtks_t *, void *, size_t,
                             size_t *offset);
  int mulib_xtks_write_all (const mulib_xtks_t *, const void *, size_t,
                            size_t *offset);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_XTKS) && !defined(MULIB_XTKS_IMPLEMENTED)
#define MULIB_XTKS_IMPLEMENTED
int
mulib_xtks_read (const mulib_xtks_t *s, void *p, size_t n, size_t *count)
{
  size_t got = 0;
  int r;
  if (count)
    *count = 0;
  if (!s || !s->read || !count || (!p && n))
    return -1;
  if (!n)
    return 0;
  r = s->read (s->context, p, n, &got);
  if (got > n || (r && got) || r > 2 || (!r && !got))
    return -2;
  if (!r)
    *count = got;
  return r;
}
int
mulib_xtks_write (const mulib_xtks_t *s, const void *p, size_t n,
                  size_t *count)
{
  size_t got = 0;
  int r;
  if (count)
    *count = 0;
  if (!s || !s->write || !count || (!p && n))
    return -1;
  if (!n)
    return 0;
  r = s->write (s->context, p, n, &got);
  if (got > n || (r && got) || r > 1 || (!r && !got))
    return -2;
  if (!r)
    *count = got;
  return r;
}
int
mulib_xtks_read_exact (const mulib_xtks_t *s, void *p, size_t n, size_t *off)
{
  if (!s || !s->read || !off || *off > n || (!p && n))
    return -1;
  while (*off < n)
    {
      size_t got;
      int r = mulib_xtks_read (s, (unsigned char *)p + *off, n - *off, &got);
      if (r)
        return r;
      *off += got;
    }
  return 0;
}
int
mulib_xtks_write_all (const mulib_xtks_t *s, const void *p, size_t n,
                      size_t *off)
{
  if (!s || !s->write || !off || *off > n || (!p && n))
    return -1;
  while (*off < n)
    {
      size_t got;
      int r = mulib_xtks_write (s, (const unsigned char *)p + *off, n - *off,
                                &got);
      if (r)
        return r;
      *off += got;
    }
  return 0;
}
#endif
