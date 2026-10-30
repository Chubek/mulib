/** @file mulib-rootfs.h
 * @brief Bounded memory root filesystem imported from the supported USTAR
 * profile. Define MULIB_IMPL_ROOTFS plus VFS, ARCHIVE and BUFFER. Never
 * extracts to host. */
#ifndef MULIB_ROOTFS_H
#define MULIB_ROOTFS_H
#include "mulib-vfs.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_rootfs
  {
    mulib_vfs_mem storage;
  } mulib_rootfs_t;
  /** @brief Initialize an empty root, using VFS node and per-file limits. 0
   * success. */
  int mulib_rootfs_init (mulib_rootfs_t *root, size_t max_nodes,
                         size_t max_file_bytes);
  /** @brief Import a complete archive into uninitialized output, atomically.
   * Parents created as needed. Files/directories only; reject duplicate
   * entries, whiteout names (.wh.*), links and unsupported archive syntax. No
   * OCI overlay whiteout semantics, ownership/permissions or compression are
   * implemented. */
  int mulib_rootfs_import (const void *tar, size_t size, size_t max_nodes,
                           size_t max_file_bytes, mulib_rootfs_t *out);
  /** @brief Release/reset. */
  void mulib_rootfs_free (mulib_rootfs_t *root);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ROOTFS) && !defined(MULIB_ROOTFS_IMPLEMENTED)
#define MULIB_ROOTFS_IMPLEMENTED
#include "mulib-archive.h"
#include <stdlib.h>
#include <string.h>
int
mulib_rootfs_init (mulib_rootfs_t *r, size_t n, size_t bytes)
{
  return r ? mulib_vfs_mem_init (&r->storage, n, bytes) : MULIB_VFS_EINVAL;
}
void
mulib_rootfs_free (mulib_rootfs_t *r)
{
  if (r)
    mulib_vfs_mem_free (&r->storage);
}
int
mulib_rootfs_import (const void *p, size_t n, size_t nodes, size_t bytes,
                     mulib_rootfs_t *out)
{
  mulib_rootfs_t root;
  mulib_archive_reader_t reader;
  mulib_archive_entry_t e;
  char path[MULIB_VFS_PATH_CAP];
  unsigned char *explicit_entries = NULL;
  size_t len, i, tracked = 0;
  int rc;
  if (!out)
    return -1;
  rc = mulib_rootfs_init (&root, nodes, bytes);
  if (rc)
    return rc;
  rc = mulib_archive_reader_init (&reader, p, n);
  if (rc)
    goto fail;
  while ((rc = mulib_archive_next (&reader, &e)) == MULIB_ARCHIVE_ENTRY)
    {
      len = strlen (e.path);
      if (len && e.path[len - 1] == '/')
        --len;
      if (!len || len + 2 > sizeof (path))
        {
          rc = -1;
          goto fail;
        }
      path[0] = '/';
      memcpy (path + 1, e.path, len);
      path[len + 1] = 0;
      /* Every component is checked for unsupported OCI whiteouts. */
      for (i = 1; i <= len; ++i)
        if ((i == 1 || path[i - 1] == '/') && !strncmp (path + i, ".wh.", 4))
          {
            rc = -1;
            goto fail;
          }
      for (i = 1; i <= len; ++i)
        if (path[i] == '/')
          {
            int a;
            path[i] = 0;
            a = mulib_vfs_mem_mkdir (&root.storage, path);
            path[i] = '/';
            if (a && a != MULIB_VFS_EEXIST)
              {
                rc = a;
                goto fail;
              }
          }
      /* VFS nodes are append-only during import. Track explicit entries by
       * node index, avoiding repeated parsing/checksumming of archive
       * prefixes. */
      for (i = 0; i < root.storage.count; ++i)
        if (!strcmp (root.storage.nodes[i].path, path))
          break;
      if (i < root.storage.count
          && ((i < tracked && explicit_entries[i])
              || root.storage.nodes[i].kind != MULIB_VFS_DIRECTORY
              || e.type != '5'))
        {
          rc = MULIB_VFS_EEXIST;
          goto fail;
        }
      if (e.type == '5')
        {
          rc = mulib_vfs_mem_mkdir (&root.storage, path);
          if (rc == MULIB_VFS_EEXIST)
            rc = 0;
        }
      else
        rc = mulib_vfs_mem_put (&root.storage, path, e.data, e.size);
      if (rc)
        goto fail;
      if (root.storage.count > tracked)
        {
          unsigned char *next = (unsigned char *)realloc (explicit_entries,
                                                          root.storage.count);
          if (!next)
            {
              rc = MULIB_VFS_ENOMEM;
              goto fail;
            }
          explicit_entries = next;
          memset (next + tracked, 0, root.storage.count - tracked);
          tracked = root.storage.count;
        }
      explicit_entries[i] = 1;
    }
  if (rc)
    goto fail;
  free (explicit_entries);
  *out = root;
  return 0;
fail:
  free (explicit_entries);
  mulib_rootfs_free (&root);
  return rc;
}
#endif
