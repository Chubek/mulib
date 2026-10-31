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
   * implemented. Returns only VFS errors; archive syntax/path errors are
   * VFS_EFORMAT, distinct from allocation and quota errors. */
  int mulib_rootfs_import (const void *tar, size_t size, size_t max_nodes,
                           size_t max_file_bytes, mulib_rootfs_t *out);
  /** @brief Preflight the archive's aggregate file bytes before allocating any
   * filesystem storage, then import with the usual path/type validation.
   * Returns VFS errors only; malformed/unsafe archives return VFS_EFORMAT. */
  int mulib_rootfs_import_limited (const void *tar, size_t size, size_t max_nodes,
      size_t max_file_bytes, size_t max_total_bytes, mulib_rootfs_t *out);
  /** @brief Deep-copy into uninitialized output; allocation failure is atomic. */
  int mulib_rootfs_clone (const mulib_rootfs_t *root, mulib_rootfs_t *out);
  /** @brief Create all missing parents. Existing directories succeed; on
   * failure previously created parents remain. Use a buildspec for atomicity. */
  int mulib_rootfs_mkdirs (mulib_rootfs_t *root, const char *path);
  /** @brief Replace a complete file, optionally creating parent directories. */
  int mulib_rootfs_put (mulib_rootfs_t *root, const char *path,
                       const void *data, size_t length, int parents);
  /** @brief Read a file range; count is set to zero before validation. */
  int mulib_rootfs_read (const mulib_rootfs_t *root, const char *path,
                        size_t offset, void *data, size_t capacity,
                        size_t *count);
  /** @brief Query a file/directory, including the implicit root. */
  int mulib_rootfs_stat (const mulib_rootfs_t *root, const char *path,
                        mulib_vfs_stat *info);
  /** @brief Remove an entry; recursive removes descendants, but never root. */
  int mulib_rootfs_remove (mulib_rootfs_t *root, const char *path, int recursive);
  /** @brief Total file bytes with checked addition. */
  int mulib_rootfs_usage (const mulib_rootfs_t *root, size_t *bytes,
                         size_t *files, size_t *directories);
  /** @brief Atomically merge a root into another. Existing files are replaced
   * only when replace is nonzero. Type conflicts always fail. */
  int mulib_rootfs_merge (mulib_rootfs_t *root, const mulib_rootfs_t *layer,
                         int replace);
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
  if (!out || (!p && n))
    return MULIB_VFS_EINVAL;
  rc = mulib_rootfs_init (&root, nodes, bytes);
  if (rc)
    return rc;
  rc = mulib_archive_reader_init (&reader, p, n);
  if (rc)
    { rc = MULIB_VFS_EFORMAT; goto fail; }
  while ((rc = mulib_archive_next (&reader, &e)) == MULIB_ARCHIVE_ENTRY)
    {
      len = strlen (e.path);
      if (len && e.path[len - 1] == '/')
        --len;
      if (!len || len + 2 > sizeof (path))
        {
          rc = MULIB_VFS_EFORMAT;
          goto fail;
        }
      path[0] = '/';
      memcpy (path + 1, e.path, len);
      path[len + 1] = 0;
      /* Every component is checked for unsupported OCI whiteouts. */
      for (i = 1; i <= len; ++i)
        if ((i == 1 || path[i - 1] == '/') && !strncmp (path + i, ".wh.", 4))
          {
            rc = MULIB_VFS_EFORMAT;
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
    { rc = MULIB_VFS_EFORMAT; goto fail; }
  free (explicit_entries);
  *out = root;
  return 0;
fail:
  free (explicit_entries);
  mulib_rootfs_free (&root);
  return rc;
}
int
mulib_rootfs_import_limited (const void *tar, size_t size, size_t nodes,
    size_t file_bytes, size_t total_bytes, mulib_rootfs_t *out)
{
  mulib_archive_reader_t reader;
  mulib_archive_entry_t entry;
  size_t total = 0;
  int rc;
  if (!out || (!tar && size)) return MULIB_VFS_EINVAL;
  rc = mulib_archive_reader_init (&reader, tar, size);
  if (rc) return MULIB_VFS_EFORMAT;
  while ((rc = mulib_archive_next (&reader, &entry)) == MULIB_ARCHIVE_ENTRY)
    {
      if (entry.size > file_bytes || entry.size > total_bytes - total) return MULIB_VFS_ELIMIT;
      total += entry.size;
    }
  if (rc) return MULIB_VFS_EFORMAT;
  return mulib_rootfs_import (tar, size, nodes, file_bytes, out);
}
int
mulib_rootfs_clone (const mulib_rootfs_t *root, mulib_rootfs_t *out)
{
  mulib_rootfs_t copy;
  size_t i;
  int rc;
  if (!root || !out || root == out || !root->storage.max_nodes)
    return MULIB_VFS_EINVAL;
  rc = mulib_rootfs_init (&copy, root->storage.max_nodes,
                          root->storage.max_file_bytes);
  if (rc) return rc;
  for (i = 0; i < root->storage.count && !rc; ++i)
    {
      const mulib_vfs_mem_node *n = root->storage.nodes + i;
      rc = n->kind == MULIB_VFS_DIRECTORY
             ? mulib_vfs_mem_mkdir (&copy.storage, n->path)
             : mulib_vfs_mem_put (&copy.storage, n->path, n->data, n->length);
    }
  if (rc) { mulib_rootfs_free (&copy); return rc; }
  *out = copy;
  return 0;
}
int
mulib_rootfs_stat (const mulib_rootfs_t *root, const char *path,
                   mulib_vfs_stat *info)
{
  mulib_vfs_ops ops = mulib_vfs_mem_ops ();
  char normalized[MULIB_VFS_PATH_CAP];
  int rc;
  if (!root || !info || !root->storage.max_nodes) return MULIB_VFS_EINVAL;
  rc = mulib_vfs_normalize (path, normalized, sizeof (normalized));
  if (rc) return rc;
  return ops.stat ((void *)&root->storage, normalized, info);
}
int
mulib_rootfs_read (const mulib_rootfs_t *root, const char *path, size_t offset,
                   void *data, size_t capacity, size_t *count)
{
  mulib_vfs_ops ops = mulib_vfs_mem_ops ();
  char normalized[MULIB_VFS_PATH_CAP];
  int rc;
  if (count) *count = 0;
  if (!root || !count || (!data && capacity) || !root->storage.max_nodes)
    return MULIB_VFS_EINVAL;
  rc = mulib_vfs_normalize (path, normalized, sizeof (normalized));
  if (rc) return rc;
  return ops.read ((void *)&root->storage, normalized, offset, data, capacity, count);
}
int
mulib_rootfs_mkdirs (mulib_rootfs_t *root, const char *path)
{
  char normalized[MULIB_VFS_PATH_CAP];
  size_t i, n;
  int rc;
  if (!root || !root->storage.max_nodes) return MULIB_VFS_EINVAL;
  rc = mulib_vfs_normalize (path, normalized, sizeof (normalized));
  if (rc || !strcmp (normalized, "/")) return rc;
  n = strlen (normalized);
  for (i = 1; i <= n; ++i)
    if (normalized[i] == '/' || i == n)
      {
        char save = normalized[i];
        mulib_vfs_stat info;
        normalized[i] = 0;
        rc = mulib_vfs_mem_mkdir (&root->storage, normalized);
        if (rc == MULIB_VFS_EEXIST)
          {
            rc = mulib_rootfs_stat (root, normalized, &info);
            if (!rc && info.kind != MULIB_VFS_DIRECTORY) rc = MULIB_VFS_ENOTDIR;
          }
        normalized[i] = save;
        if (rc) return rc;
      }
  return 0;
}
int
mulib_rootfs_put (mulib_rootfs_t *root, const char *path, const void *data,
                  size_t length, int parents)
{
  char normalized[MULIB_VFS_PATH_CAP], *slash;
  int rc;
  if (!root || (!data && length)) return MULIB_VFS_EINVAL;
  rc = mulib_vfs_normalize (path, normalized, sizeof (normalized));
  if (rc) return rc;
  if (parents && (slash = strrchr (normalized, '/')) != normalized)
    {
      *slash = 0; rc = mulib_rootfs_mkdirs (root, normalized); *slash = '/';
      if (rc) return rc;
    }
  return mulib_vfs_mem_put (&root->storage, normalized, data, length);
}
int
mulib_rootfs_remove (mulib_rootfs_t *root, const char *path, int recursive)
{
  char normalized[MULIB_VFS_PATH_CAP];
  mulib_vfs_stat info;
  size_t i, n;
  int rc;
  if (!root) return MULIB_VFS_EINVAL;
  rc = mulib_vfs_normalize (path, normalized, sizeof (normalized));
  if (rc) return rc;
  if (!strcmp (normalized, "/")) return MULIB_VFS_EINVAL;
  rc = mulib_rootfs_stat (root, normalized, &info);
  if (rc) return rc;
  n = strlen (normalized);
  if (recursive)
    for (i = root->storage.count; i; --i)
      if (!strncmp (root->storage.nodes[i - 1].path, normalized, n)
          && root->storage.nodes[i - 1].path[n] == '/')
        {
          rc = mulib_vfs_mem_remove (&root->storage, root->storage.nodes[i - 1].path);
          if (rc) return rc;
        }
  return mulib_vfs_mem_remove (&root->storage, normalized);
}
int
mulib_rootfs_usage (const mulib_rootfs_t *root, size_t *bytes, size_t *files,
                    size_t *directories)
{
  size_t i, b = 0, f = 0, d = 0;
  if (!root || !root->storage.max_nodes) return MULIB_VFS_EINVAL;
  for (i = 0; i < root->storage.count; ++i)
    if (root->storage.nodes[i].kind == MULIB_VFS_FILE)
      {
        if (root->storage.nodes[i].length > SIZE_MAX - b) return MULIB_VFS_ELIMIT;
        b += root->storage.nodes[i].length; ++f;
      }
    else ++d;
  if (bytes) *bytes = b;
  if (files) *files = f;
  if (directories) *directories = d;
  return 0;
}
int
mulib_rootfs_merge (mulib_rootfs_t *root, const mulib_rootfs_t *layer, int replace)
{
  mulib_rootfs_t copy;
  size_t i;
  int rc;
  if (!root || !layer || !layer->storage.max_nodes) return MULIB_VFS_EINVAL;
  rc = mulib_rootfs_clone (root, &copy);
  if (rc) return rc;
  for (i = 0; i < layer->storage.count && !rc; ++i)
    {
      const mulib_vfs_mem_node *n = layer->storage.nodes + i;
      mulib_vfs_stat info;
      int found = mulib_rootfs_stat (&copy, n->path, &info);
      if (!found && (info.kind != n->kind
                     || (n->kind == MULIB_VFS_FILE && !replace)))
        { rc = MULIB_VFS_EEXIST; break; }
      if (found && found != MULIB_VFS_ENOENT) { rc = found; break; }
      rc = n->kind == MULIB_VFS_DIRECTORY
             ? mulib_rootfs_mkdirs (&copy, n->path)
             : mulib_rootfs_put (&copy, n->path, n->data, n->length, 1);
    }
  if (rc) { mulib_rootfs_free (&copy); return rc; }
  mulib_rootfs_free (root); *root = copy;
  return 0;
}
#endif
