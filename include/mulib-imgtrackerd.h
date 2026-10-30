/** @file mulib-imgtrackerd.h
 * @brief In-memory reference counts for OCI sha256 image digests, not a
 * daemon. Define MULIB_IMPL_IMGTRACKERD plus OCIIMAGE, MEMTABLE and BYCUTILS
 * dependencies. */
#ifndef MULIB_IMGTRACKERD_H
#define MULIB_IMGTRACKERD_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_imgtrackerd
  {
    mulib_memtable_t images;
  } mulib_imgtrackerd_t;
  /** @brief Zero init. Retain digest, 0 success, negative
   * invalid/OOM/overflow. */
  int mulib_imgtrackerd_retain (mulib_imgtrackerd_t *t, const char *digest);
  /** @brief Release a reference; 0 success, 1 not retained, negative error. */
  int mulib_imgtrackerd_release (mulib_imgtrackerd_t *t, const char *digest);
  /** @brief Reference count; zero absent/invalid. Does not authenticate hash.
   */
  uint64_t mulib_imgtrackerd_count (const mulib_imgtrackerd_t *t,
                                    const char *digest);
  /** @brief Free/reset, no garbage collection of external blobs. */
  void mulib_imgtrackerd_free (mulib_imgtrackerd_t *t);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_IMGTRACKERD) && !defined(MULIB_IMGTRACKERD_IMPLEMENTED)
#define MULIB_IMGTRACKERD_IMPLEMENTED
#include "mulib-bycutils.h"
#include "mulib-ociimage.h"
uint64_t
mulib_imgtrackerd_count (const mulib_imgtrackerd_t *t, const char *d)
{
  const mulib_memtable_record_t *r;
  uint64_t n = 0;
  if (t && mulib_ociimage_digest (d)
      && !mulib_memtable_get (&t->images, d, 71, &r))
    mulib_bycutils_load (r->value, r->value_size, 0, 8, &n);
  return n;
}
static int
mulib_imgtrackerd_change_ (mulib_imgtrackerd_t *t, const char *d, int add)
{
  uint64_t n;
  unsigned char b[8];
  if (!t || !mulib_ociimage_digest (d))
    return -1;
  n = mulib_imgtrackerd_count (t, d);
  if (add)
    {
      if (n == UINT64_MAX)
        return -1;
      ++n;
    }
  else
    {
      if (!n)
        return 1;
      --n;
    }
  mulib_bycutils_store (b, 8, 0, 8, n);
  return mulib_memtable_put (&t->images, d, 71, b, 8, 0, 0);
}
int
mulib_imgtrackerd_retain (mulib_imgtrackerd_t *t, const char *d)
{
  return mulib_imgtrackerd_change_ (t, d, 1);
}
int
mulib_imgtrackerd_release (mulib_imgtrackerd_t *t, const char *d)
{
  return mulib_imgtrackerd_change_ (t, d, 0);
}
void
mulib_imgtrackerd_free (mulib_imgtrackerd_t *t)
{
  if (t)
    mulib_memtable_free (&t->images);
}
#endif
