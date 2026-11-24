/** @file mulib-binfile.h
 * @brief Borrowed binary file spans and format identification, without I/O.
 * Define MULIB_IMPL_BINFILE once. */
#ifndef MULIB_BINFILE_H
#define MULIB_BINFILE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_binfile
  {
    const unsigned char *data;
    size_t size;
  } mulib_binfile_t;
  enum
  {
    MULIB_BINFILE_UNKNOWN = 0,
    MULIB_BINFILE_ELF = 1,
    MULIB_BINFILE_AR = 2
  };
  /** @brief Open borrowed bytes; NULL allowed only for empty span. 1 success.
   */
  int mulib_binfile_open (mulib_binfile_t *out, const void *data, size_t size);
  /** @brief Slice with subtraction-based bounds checking; output unchanged on
   * error. */
  int mulib_binfile_slice (const mulib_binfile_t *file, size_t offset,
                           size_t size, mulib_binfile_t *out);
  /** @brief Identify magic only, not structural validity. */
  int mulib_binfile_format (const mulib_binfile_t *file);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BINFILE) && !defined(MULIB_BINFILE_IMPLEMENTED)
#define MULIB_BINFILE_IMPLEMENTED
#include <string.h>
int
mulib_binfile_open (mulib_binfile_t *out, const void *p, size_t n)
{
  if (!out || (!p && n))
    return 0;
  out->data = (const unsigned char *)p;
  out->size = n;
  return 1;
}
int
mulib_binfile_slice (const mulib_binfile_t *f, size_t o, size_t n,
                     mulib_binfile_t *out)
{
  mulib_binfile_t v;
  if (!f || !out || (!f->data && f->size) || o > f->size || n > f->size - o)
    return 0;
  v.data = f->data ? f->data + o : NULL;
  v.size = n;
  *out = v;
  return 1;
}
int
mulib_binfile_format (const mulib_binfile_t *f)
{
  if (!f || !f->data)
    return 0;
  if (f->size >= 4 && !memcmp (f->data, "\177ELF", 4))
    return MULIB_BINFILE_ELF;
  if (f->size >= 8 && !memcmp (f->data, "!<arch>\n", 8))
    return MULIB_BINFILE_AR;
  return 0;
}
#endif
