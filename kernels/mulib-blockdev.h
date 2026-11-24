/* MuLIB: Application-supplied fixed-size block device with checked I/O callbacks. C11
 * single-header; define MULIB_IMPL_BLOCKDEV once. */
#ifndef MULIB_BLOCKDEV_H
#define MULIB_BLOCKDEV_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef int (*mulib_blockdev_io_fn) (void *context, uint64_t block, void *buffer,
                                       size_t size);
  typedef int (*mulib_blockdev_flush_fn) (void *context);
  typedef struct
  {
    void *context;
    size_t block_size;
    mulib_blockdev_io_fn read, write;
    mulib_blockdev_flush_fn flush;
  } mulib_blockdev_t;
  int mulib_blockdev_read (const mulib_blockdev_t *d, uint64_t block, void *out);
  int mulib_blockdev_write (const mulib_blockdev_t *d, uint64_t block, const void *in);
  int mulib_blockdev_flush (const mulib_blockdev_t *d);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BLOCKDEV) && !defined(MULIB_BLOCKDEV_IMPLEMENTED)
#define MULIB_BLOCKDEV_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_blockdev_read (const mulib_blockdev_t *d, uint64_t b, void *out)
  {
    return d && d->read && d->block_size && out
               ? d->read (d->context, b, out, d->block_size)
               : 0;
  }
  int
  mulib_blockdev_write (const mulib_blockdev_t *d, uint64_t b, const void *in)
  {
    return d && d->write && d->block_size && in
               ? d->write (d->context, b, (void *)in, d->block_size)
               : 0;
  }
  int
  mulib_blockdev_flush (const mulib_blockdev_t *d)
  {
    return d ? (d->flush ? d->flush (d->context) : 1) : 0;
  }
#ifdef __cplusplus
}
#endif
#endif
