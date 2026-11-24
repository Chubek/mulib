/** @file mulib-containerd-mount.h
 * @brief Daemon VFS exports with stable generation-tagged leases and FUSE sessions.
 * Define MULIB_IMPL_CONTAINERD_MOUNT plus CONTAINER_VFS/FUSE dependencies. */
#ifndef MULIB_CONTAINERD_MOUNT_H
#define MULIB_CONTAINERD_MOUNT_H
#include "mulib-container-vfs.h"
#include "mulib-fuse.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_containerd_mount_lease
{ size_t slot; uint64_t generation; } mulib_containerd_mount_lease;
typedef struct mulib_containerd_mount_entry
{
  char id[MULIB_CT_ID_CAP];
  mulib_container_vfs filesystem;
  mulib_fuse_session session;
  uint64_t generation;
  size_t references;
  int active;
} mulib_containerd_mount_entry;
typedef struct mulib_containerd_mount
{
  mulib_containerd_mount_entry *entries;
  size_t capacity, count;
  uint64_t generation;
} mulib_containerd_mount;
/** @brief Allocate a stable bounded export table. */
int mulib_containerd_mount_init (mulib_containerd_mount *mounts, size_t capacity);
/** @brief Free exports only if every lease is released and session idle. */
int mulib_containerd_mount_free (mulib_containerd_mount *mounts);
/** @brief Export a borrowed root, with per-export mount and transfer limits. */
int mulib_containerd_mount_add (mulib_containerd_mount *mounts, const char *id,
    mulib_rootfs_t *root, int read_only, size_t max_bindings, size_t max_transfer);
/** @brief Pin an export and borrow its filesystem until release. */
int mulib_containerd_mount_acquire (mulib_containerd_mount *mounts, const char *id,
    mulib_containerd_mount_lease *lease, mulib_container_vfs **filesystem);
/** @brief Release exactly one acquired reference. */
int mulib_containerd_mount_release (mulib_containerd_mount *mounts, mulib_containerd_mount_lease lease);
/** @brief Unexport only when unpinned; root remains caller-owned. */
int mulib_containerd_mount_remove (mulib_containerd_mount *mounts, const char *id);
/** @brief Dispatch one typed filesystem request through a live pinned lease.
 * Return value and response use the native VFS error family. */
int mulib_containerd_mount_dispatch (mulib_containerd_mount *mounts,
    mulib_containerd_mount_lease lease, const mulib_fuse_request *request,
    void *output, size_t capacity, mulib_vfs_visit visit, void *user,
    mulib_fuse_response *response);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINERD_MOUNT) && !defined(MULIB_CONTAINERD_MOUNT_IMPLEMENTED)
#define MULIB_CONTAINERD_MOUNT_IMPLEMENTED
int mulib_containerd_mount_init (mulib_containerd_mount *m, size_t capacity)
{
  mulib_containerd_mount next;
  if (!m || !capacity || capacity > SIZE_MAX / sizeof (*next.entries)) return MULIB_CT_EINVAL;
  memset (&next, 0, sizeof (next));
  next.entries = (mulib_containerd_mount_entry *)MULIB_CT_MALLOC (capacity * sizeof (*next.entries));
  if (!next.entries) return MULIB_CT_ENOMEM;
  memset (next.entries, 0, capacity * sizeof (*next.entries)); next.capacity = capacity; *m = next;
  return 0;
}
int mulib_containerd_mount_free (mulib_containerd_mount *m)
{
  size_t i;
  if (!m) return MULIB_CT_EINVAL;
  for (i = 0; i < m->capacity; ++i)
    if (m->entries[i].references || m->entries[i].session.busy) return MULIB_CT_EBUSY;
  for (i = 0; i < m->capacity; ++i)
    if (m->entries[i].active) mulib_container_vfs_free (&m->entries[i].filesystem);
  MULIB_CT_FREE (m->entries); memset (m, 0, sizeof (*m));
  return 0;
}
int
mulib_containerd_mount_add (mulib_containerd_mount *m, const char *id,
    mulib_rootfs_t *root, int ro, size_t bindings, size_t transfer)
{
  size_t i, slot = SIZE_MAX;
  mulib_containerd_mount_entry *e;
  int rc;
  if (!m || !mulib_ct_id_valid (id)) return MULIB_CT_EINVAL;
  for (i = 0; i < m->capacity; ++i)
    if (m->entries[i].active)
      { if (!strcmp (m->entries[i].id, id)) return MULIB_CT_EEXIST; }
    else if (slot == SIZE_MAX) slot = i;
  if (slot == SIZE_MAX || m->generation == UINT64_MAX) return MULIB_CT_ELIMIT;
  e = m->entries + slot;
  rc = mulib_container_vfs_init (&e->filesystem, root, bindings, ro);
  if (rc) return rc == MULIB_VFS_ENOMEM ? MULIB_CT_ENOMEM : MULIB_CT_EINVAL;
  rc = mulib_fuse_session_init (&e->session, &e->filesystem.router, ro, transfer);
  if (rc) { mulib_container_vfs_free (&e->filesystem); return MULIB_CT_EIO; }
  (void)mulib_ct_text_copy (e->id, sizeof (e->id), id);
  e->generation = ++m->generation; e->active = 1; ++m->count;
  return 0;
}
int
mulib_containerd_mount_acquire (mulib_containerd_mount *m, const char *id,
    mulib_containerd_mount_lease *lease, mulib_container_vfs **filesystem)
{
  size_t i;
  if (!m || !id || !lease || !filesystem) return MULIB_CT_EINVAL;
  for (i = 0; i < m->capacity; ++i)
    if (m->entries[i].active && !strcmp (m->entries[i].id, id))
      {
        if (m->entries[i].references == SIZE_MAX) return MULIB_CT_ELIMIT;
        ++m->entries[i].references; lease->slot = i; lease->generation = m->entries[i].generation;
        *filesystem = &m->entries[i].filesystem; return 0;
      }
  return MULIB_CT_ENOENT;
}
static mulib_containerd_mount_entry *
mulib_containerd_mount_entry_ (mulib_containerd_mount *m, mulib_containerd_mount_lease l)
{
  if (!m || l.slot >= m->capacity || !m->entries[l.slot].active
      || !m->entries[l.slot].references || m->entries[l.slot].generation != l.generation) return NULL;
  return m->entries + l.slot;
}
int mulib_containerd_mount_release (mulib_containerd_mount *m, mulib_containerd_mount_lease l)
{
  mulib_containerd_mount_entry *e = mulib_containerd_mount_entry_ (m, l);
  if (!e) return MULIB_CT_ESTALE;
  if (e->session.busy) return MULIB_CT_EBUSY;
  --e->references;
  return 0;
}
int mulib_containerd_mount_remove (mulib_containerd_mount *m, const char *id)
{
  size_t i;
  if (!m || !id) return MULIB_CT_EINVAL;
  for (i = 0; i < m->capacity; ++i)
    if (m->entries[i].active && !strcmp (m->entries[i].id, id))
      {
        if (m->entries[i].references || m->entries[i].session.busy) return MULIB_CT_EBUSY;
        mulib_container_vfs_free (&m->entries[i].filesystem);
        memset (m->entries + i, 0, sizeof (*m->entries)); --m->count;
        return 0;
      }
  return MULIB_CT_ENOENT;
}
int
mulib_containerd_mount_dispatch (mulib_containerd_mount *m, mulib_containerd_mount_lease l,
    const mulib_fuse_request *q, void *out, size_t cap, mulib_vfs_visit visit,
    void *user, mulib_fuse_response *response)
{
  mulib_containerd_mount_entry *e = mulib_containerd_mount_entry_ (m, l);
  if (!e)
    {
      if (response)
        { memset (response, 0, sizeof (*response)); response->id = q ? q->id : 0; response->result = MULIB_VFS_ENOENT; }
      return MULIB_VFS_ENOENT;
    }
  return mulib_fuse_session_dispatch (&e->session, q, out, cap, visit, user, response);
}
#endif
