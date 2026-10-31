/** @file mulib-container-vfs.h
 * @brief Per-container VFS routing, read-only bindings and root checkpoints.
 * Define MULIB_IMPL_CONTAINER_VFS plus ROOTFS/VFS dependencies. */
#ifndef MULIB_CONTAINER_VFS_H
#define MULIB_CONTAINER_VFS_H
#include "include-mulib-container-core.h"
#include "mulib-rootfs.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_container_vfs_binding
{
  char path[MULIB_VFS_PATH_CAP];
  mulib_vfs_ops operations;
  void *context;
  int read_only;
} mulib_container_vfs_binding;
/** @brief Owns router/binding wrappers; root and backend contexts are borrowed.
 * Keep the initialized object at a stable address until free. */
typedef struct mulib_container_vfs
{
  mulib_vfs router;
  mulib_rootfs_t *root;
  mulib_container_vfs_binding **bindings;
  size_t count, limit;
  uint64_t generation;
  int read_only;
} mulib_container_vfs;
/** @brief Mount root at /; max_mounts includes root. Returns VFS errors. */
int mulib_container_vfs_init (mulib_container_vfs *filesystem, mulib_rootfs_t *root,
                             size_t max_mounts, int read_only);
/** @brief Unmount and free wrappers, never borrowed backend storage. */
void mulib_container_vfs_free (mulib_container_vfs *filesystem);
/** @brief Copy operations and mount path. Whole-filesystem read-only dominates. */
int mulib_container_vfs_bind (mulib_container_vfs *filesystem, const char *path,
                              const mulib_vfs_ops *operations, void *context, int read_only);
/** @brief Remove exact non-root mount, leaving nested mounts intact. */
int mulib_container_vfs_unbind (mulib_container_vfs *filesystem, const char *path);
/** @brief Clone only the root storage, excluding other mounted backends. */
int mulib_container_vfs_checkpoint (const mulib_container_vfs *filesystem, mulib_rootfs_t *out);
/** @brief Atomically replace root from a deep-copied checkpoint. Existing root
 * limits remain authoritative. Borrowed mounts and routing are preserved. */
int mulib_container_vfs_restore (mulib_container_vfs *filesystem, const mulib_rootfs_t *checkpoint);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINER_VFS) && !defined(MULIB_CONTAINER_VFS_IMPLEMENTED)
#define MULIB_CONTAINER_VFS_IMPLEMENTED
static mulib_vfs_result mulib_container_vfs_read_ (void *ctx, const char *p,
    size_t off, void *data, size_t n, size_t *count)
{
  mulib_container_vfs_binding *b = (mulib_container_vfs_binding *)ctx;
  return b->operations.read ? b->operations.read (b->context, p, off, data, n, count) : MULIB_VFS_ENOTSUP;
}
static mulib_vfs_result mulib_container_vfs_write_ (void *ctx, const char *p,
    size_t off, const void *data, size_t n, size_t *count)
{
  mulib_container_vfs_binding *b = (mulib_container_vfs_binding *)ctx;
  return b->read_only || !b->operations.write ? MULIB_VFS_EROFS
      : b->operations.write (b->context, p, off, data, n, count);
}
static mulib_vfs_result mulib_container_vfs_stat_ (void *ctx, const char *p, mulib_vfs_stat *s)
{
  mulib_container_vfs_binding *b = (mulib_container_vfs_binding *)ctx;
  return b->operations.stat ? b->operations.stat (b->context, p, s) : MULIB_VFS_ENOTSUP;
}
static mulib_vfs_result mulib_container_vfs_list_ (void *ctx, const char *p,
                                                 mulib_vfs_visit visit, void *user)
{
  mulib_container_vfs_binding *b = (mulib_container_vfs_binding *)ctx;
  return b->operations.list ? b->operations.list (b->context, p, visit, user) : MULIB_VFS_ENOTSUP;
}
int
mulib_container_vfs_bind (mulib_container_vfs *fs, const char *path,
                          const mulib_vfs_ops *ops, void *ctx, int ro)
{
  mulib_container_vfs_binding *b;
  mulib_vfs_ops wrapped;
  int rc;
  if (!fs || !ops || !fs->limit) return MULIB_VFS_EINVAL;
  if (fs->count >= fs->limit || fs->generation == UINT64_MAX) return MULIB_VFS_ELIMIT;
  b = (mulib_container_vfs_binding *)MULIB_CT_MALLOC (sizeof (*b));
  if (!b) return MULIB_VFS_ENOMEM;
  rc = mulib_vfs_normalize (path, b->path, sizeof (b->path));
  if (rc) { MULIB_CT_FREE (b); return rc; }
  b->operations = *ops; b->context = ctx; b->read_only = ro || fs->read_only;
  wrapped.read = mulib_container_vfs_read_; wrapped.write = mulib_container_vfs_write_;
  wrapped.stat = mulib_container_vfs_stat_; wrapped.list = mulib_container_vfs_list_;
  rc = mulib_vfs_mount (&fs->router, b->path, &wrapped, b);
  if (rc) { MULIB_CT_FREE (b); return rc; }
  fs->bindings[fs->count++] = b; ++fs->generation;
  return 0;
}
int
mulib_container_vfs_init (mulib_container_vfs *fs, mulib_rootfs_t *root, size_t max, int ro)
{
  mulib_container_vfs next;
  mulib_vfs_ops ops = mulib_vfs_mem_ops ();
  int rc;
  if (!fs || !root || !root->storage.max_nodes || !max
      || max > SIZE_MAX / sizeof (*next.bindings)) return MULIB_VFS_EINVAL;
  memset (&next, 0, sizeof (next));
  rc = mulib_vfs_init (&next.router, max);
  if (rc) return rc;
  next.bindings = (mulib_container_vfs_binding **)MULIB_CT_MALLOC (max * sizeof (*next.bindings));
  if (!next.bindings) { mulib_vfs_free (&next.router); return MULIB_VFS_ENOMEM; }
  next.limit = max; next.root = root; next.read_only = ro != 0;
  rc = mulib_container_vfs_bind (&next, "/", &ops, &root->storage, ro);
  if (rc) { mulib_container_vfs_free (&next); return rc; }
  *fs = next;
  return 0;
}
void mulib_container_vfs_free (mulib_container_vfs *fs)
{
  size_t i;
  if (!fs) return;
  mulib_vfs_free (&fs->router);
  for (i = 0; i < fs->count; ++i) MULIB_CT_FREE (fs->bindings[i]);
  MULIB_CT_FREE (fs->bindings); memset (fs, 0, sizeof (*fs));
}
int mulib_container_vfs_unbind (mulib_container_vfs *fs, const char *path)
{
  char p[MULIB_VFS_PATH_CAP];
  size_t i;
  int rc;
  if (!fs) return MULIB_VFS_EINVAL;
  rc = mulib_vfs_normalize (path, p, sizeof (p));
  if (rc) return rc;
  if (!strcmp (p, "/")) return MULIB_VFS_EINVAL;
  if (fs->generation == UINT64_MAX) return MULIB_VFS_ELIMIT;
  for (i = 0; i < fs->count; ++i)
    if (!strcmp (p, fs->bindings[i]->path))
      {
        rc = mulib_vfs_unmount (&fs->router, p);
        if (rc) return rc;
        MULIB_CT_FREE (fs->bindings[i]);
        memmove (fs->bindings + i, fs->bindings + i + 1,
                 (--fs->count - i) * sizeof (*fs->bindings));
        ++fs->generation; return 0;
      }
  return MULIB_VFS_ENOENT;
}
int mulib_container_vfs_checkpoint (const mulib_container_vfs *fs, mulib_rootfs_t *out)
{ return fs && fs->root ? mulib_rootfs_clone (fs->root, out) : MULIB_VFS_EINVAL; }
int mulib_container_vfs_restore (mulib_container_vfs *fs, const mulib_rootfs_t *checkpoint)
{
  mulib_rootfs_t next;
  int rc;
  if (!fs || !fs->root || !checkpoint) return MULIB_VFS_EINVAL;
  if (fs->read_only) return MULIB_VFS_EROFS;
  if (fs->generation == UINT64_MAX) return MULIB_VFS_ELIMIT;
  rc = mulib_rootfs_init (&next, fs->root->storage.max_nodes, fs->root->storage.max_file_bytes);
  if (rc) return rc;
  rc = mulib_rootfs_merge (&next, checkpoint, 1);
  if (rc) { mulib_rootfs_free (&next); return rc; }
  mulib_rootfs_free (fs->root); *fs->root = next; ++fs->generation;
  return 0;
}
#endif
