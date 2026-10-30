/** @file mulib-rootfsbuilder.h
 * @brief Deterministic USTAR export from the bounded memory root filesystem.
 * Define MULIB_IMPL_ROOTFSBUILDER plus ROOTFS, VFS, ARCHIVE and BUFFER. */
#ifndef MULIB_ROOTFSBUILDER_H
#define MULIB_ROOTFSBUILDER_H
#include "mulib-buffer.h"
#include "mulib-rootfs.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Append complete archive atomically, insertion-order entries,
   * uid/gid/mtime=0, modes 0755 directories/0644 files. Returns 0 success,
   * negative error. Long paths outside USTAR writer profile are rejected. Does
   * not consume root. */
  int mulib_rootfsbuilder_export (const mulib_rootfs_t *root,
                                  mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ROOTFSBUILDER)                                         \
    && !defined(MULIB_ROOTFSBUILDER_IMPLEMENTED)
#define MULIB_ROOTFSBUILDER_IMPLEMENTED
#include "mulib-archive.h"
int
mulib_rootfsbuilder_export (const mulib_rootfs_t *r, mulib_buffer_t *out)
{
  mulib_archive_writer_t w;
  size_t i;
  int rc = 0;
  if (!r || !out || !r->storage.max_nodes)
    return -1;
  mulib_archive_writer_init (&w);
  for (i = 0; i < r->storage.count && !rc; ++i)
    {
      const mulib_vfs_mem_node *n = r->storage.nodes + i;
      rc = n->kind == MULIB_VFS_DIRECTORY
               ? mulib_archive_add_directory (&w, n->path + 1, 0755, 0)
               : mulib_archive_add (&w, n->path + 1, n->data, n->length, 0644,
                                    0);
    }
  if (!rc)
    rc = mulib_archive_finish (&w);
  if (!rc && !mulib_buffer_append (out, w.bytes.data, w.bytes.length))
    rc = MULIB_ARCHIVE_ENOMEM;
  mulib_archive_writer_free (&w);
  return rc;
}
#endif
