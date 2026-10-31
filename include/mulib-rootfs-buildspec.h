/** @file mulib-rootfs-buildspec.h
 * @brief Owned filesystem recipes with transactional application and quotas.
 * Define MULIB_IMPL_ROOTFS_BUILDSPEC and the ROOTFS dependency switches. */
#ifndef MULIB_ROOTFS_BUILDSPEC_H
#define MULIB_ROOTFS_BUILDSPEC_H
#include "include-mulib-container-core.h"
#include "mulib-rootfs.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_rootfs_buildop
{
  MULIB_ROOTFS_BUILD_DIRECTORY = 1, MULIB_ROOTFS_BUILD_FILE,
  MULIB_ROOTFS_BUILD_REMOVE, MULIB_ROOTFS_BUILD_COPY
} mulib_rootfs_buildop;
typedef struct mulib_rootfs_buildstep
{
  mulib_rootfs_buildop operation;
  char *path, *source;
  mulib_ct_blob bytes;
  int recursive;
} mulib_rootfs_buildstep;
/** @brief Owns ordered steps. Limits count payload and path bytes. */
typedef struct mulib_rootfs_buildspec
{
  mulib_rootfs_buildstep *steps;
  size_t count, max_steps, bytes, max_bytes, max_result_bytes;
} mulib_rootfs_buildspec;
/** @brief Initialize an empty recipe; result quota is checked after every step. */
int mulib_rootfs_buildspec_init (mulib_rootfs_buildspec *spec, size_t max_steps,
                                size_t max_bytes, size_t max_result_bytes);
/** @brief Release copied paths, sources and file contents. */
void mulib_rootfs_buildspec_free (mulib_rootfs_buildspec *spec);
/** @brief Append a step, normalizing paths and copying all data atomically.
 * COPY reads the current recipe result, REMOVE optionally removes descendants.
 * Non-FILE data must be NULL/zero; source is used only for COPY. */
int mulib_rootfs_buildspec_add (mulib_rootfs_buildspec *spec,
                               mulib_rootfs_buildop operation, const char *path,
                               const char *source, const void *data,
                               size_t length, int recursive);
/** @brief Deep-copy into uninitialized output. */
int mulib_rootfs_buildspec_clone (const mulib_rootfs_buildspec *spec,
                                 mulib_rootfs_buildspec *out);
/** @brief Apply to a clone, replacing root only on complete success. failed_step
 * receives SIZE_MAX on success or preflight failure. Native VFS errors are
 * reported in detail; the return value uses MULIB_CT_* exclusively. */
int mulib_rootfs_buildspec_apply (const mulib_rootfs_buildspec *spec,
                                 mulib_rootfs_t *root, size_t *failed_step,
                                 int *detail);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ROOTFS_BUILDSPEC) && !defined(MULIB_ROOTFS_BUILDSPEC_IMPLEMENTED)
#define MULIB_ROOTFS_BUILDSPEC_IMPLEMENTED
int
mulib_rootfs_buildspec_init (mulib_rootfs_buildspec *s, size_t steps,
                            size_t bytes, size_t result)
{
  if (!s || !steps || steps > SIZE_MAX / sizeof (*s->steps)) return MULIB_CT_EINVAL;
  memset (s, 0, sizeof (*s));
  s->max_steps = steps; s->max_bytes = bytes; s->max_result_bytes = result;
  return 0;
}
void
mulib_rootfs_buildspec_free (mulib_rootfs_buildspec *s)
{
  size_t i;
  if (!s) return;
  for (i = 0; i < s->count; ++i)
    {
      MULIB_CT_FREE (s->steps[i].path); MULIB_CT_FREE (s->steps[i].source);
      mulib_ct_blob_free (&s->steps[i].bytes);
    }
  MULIB_CT_FREE (s->steps); memset (s, 0, sizeof (*s));
}
int
mulib_rootfs_buildspec_add (mulib_rootfs_buildspec *s, mulib_rootfs_buildop op,
                           const char *path, const char *source,
                           const void *data, size_t length, int recursive)
{
  char p[MULIB_VFS_PATH_CAP], q[MULIB_VFS_PATH_CAP];
  mulib_rootfs_buildstep step, *next;
  size_t a, b = 0, cost;
  int rc;
  if (!s || op < MULIB_ROOTFS_BUILD_DIRECTORY || op > MULIB_ROOTFS_BUILD_COPY
      || (!data && length) || (op != MULIB_ROOTFS_BUILD_FILE && (data || length))
      || (op != MULIB_ROOTFS_BUILD_COPY && source)
      || (recursive && op != MULIB_ROOTFS_BUILD_REMOVE)) return MULIB_CT_EINVAL;
  if (s->count >= s->max_steps || s->bytes > s->max_bytes) return MULIB_CT_ELIMIT;
  if (mulib_vfs_normalize (path, p, sizeof (p))) return MULIB_CT_EINVAL;
  if (op != MULIB_ROOTFS_BUILD_DIRECTORY && !strcmp (p, "/")) return MULIB_CT_EINVAL;
  if (op == MULIB_ROOTFS_BUILD_COPY)
    {
      if (mulib_vfs_normalize (source, q, sizeof (q))) return MULIB_CT_EINVAL;
      b = strlen (q) + 1;
    }
  a = strlen (p) + 1;
  if (mulib_ct_add_size (a, b, &cost) || mulib_ct_add_size (cost, length, &cost)
      || cost > s->max_bytes - s->bytes) return MULIB_CT_ELIMIT;
  memset (&step, 0, sizeof (step));
  step.path = (char *)MULIB_CT_MALLOC (a);
  if (!step.path) return MULIB_CT_ENOMEM;
  memcpy (step.path, p, a);
  if (b)
    {
      step.source = (char *)MULIB_CT_MALLOC (b);
      if (!step.source) { MULIB_CT_FREE (step.path); return MULIB_CT_ENOMEM; }
      memcpy (step.source, q, b);
    }
  rc = mulib_ct_blob_assign (&step.bytes, data, length, s->max_bytes);
  if (rc) goto fail;
  next = (mulib_rootfs_buildstep *)MULIB_CT_REALLOC (
      s->steps, (s->count + 1) * sizeof (*next));
  if (!next) { rc = MULIB_CT_ENOMEM; goto fail; }
  step.operation = op; step.recursive = recursive != 0;
  s->steps = next; s->steps[s->count++] = step; s->bytes += cost;
  return 0;
fail:
  MULIB_CT_FREE (step.path); MULIB_CT_FREE (step.source);
  mulib_ct_blob_free (&step.bytes);
  return rc;
}
int
mulib_rootfs_buildspec_clone (const mulib_rootfs_buildspec *s,
                             mulib_rootfs_buildspec *out)
{
  mulib_rootfs_buildspec copy;
  size_t i;
  int rc;
  if (!s || !out || s == out) return MULIB_CT_EINVAL;
  rc = mulib_rootfs_buildspec_init (&copy, s->max_steps, s->max_bytes,
                                   s->max_result_bytes);
  if (rc) return rc;
  for (i = 0; i < s->count && !rc; ++i)
    {
      const mulib_rootfs_buildstep *p = s->steps + i;
      rc = mulib_rootfs_buildspec_add (&copy, p->operation, p->path, p->source,
                                       p->bytes.data, p->bytes.length, p->recursive);
    }
  if (rc) { mulib_rootfs_buildspec_free (&copy); return rc; }
  *out = copy;
  return 0;
}
int
mulib_rootfs_buildspec_apply (const mulib_rootfs_buildspec *s, mulib_rootfs_t *root,
                             size_t *failed, int *detail)
{
  mulib_rootfs_t copy;
  size_t i, bytes = 0;
  int rc;
  if (failed) *failed = SIZE_MAX;
  if (detail) *detail = 0;
  if (!s || !root || !s->max_steps) return MULIB_CT_EINVAL;
  rc = mulib_rootfs_clone (root, &copy);
  if (rc)
    {
      if (detail) *detail = rc;
      return rc == MULIB_VFS_ENOMEM ? MULIB_CT_ENOMEM
             : rc == MULIB_VFS_ELIMIT ? MULIB_CT_ELIMIT : MULIB_CT_EIO;
    }
  for (i = 0; i < s->count && !rc; ++i)
    {
      const mulib_rootfs_buildstep *p = s->steps + i;
      switch (p->operation)
        {
        case MULIB_ROOTFS_BUILD_DIRECTORY: rc = mulib_rootfs_mkdirs (&copy, p->path); break;
        case MULIB_ROOTFS_BUILD_FILE:
          rc = mulib_rootfs_put (&copy, p->path, p->bytes.data, p->bytes.length, 1); break;
        case MULIB_ROOTFS_BUILD_REMOVE:
          rc = mulib_rootfs_remove (&copy, p->path, p->recursive); break;
        case MULIB_ROOTFS_BUILD_COPY:
          {
            mulib_vfs_stat info;
            mulib_ct_blob data = { NULL, 0, 0 };
            size_t read = 0;
            rc = mulib_rootfs_stat (&copy, p->source, &info);
            if (!rc && info.kind != MULIB_VFS_FILE) rc = MULIB_VFS_EISDIR;
            if (!rc && info.size > s->max_result_bytes) rc = MULIB_VFS_ELIMIT;
            if (!rc && mulib_ct_blob_reserve (&data, (size_t)info.size,
                                              s->max_result_bytes)) rc = MULIB_VFS_ENOMEM;
            if (!rc) rc = mulib_rootfs_read (&copy, p->source, 0, data.data,
                                             (size_t)info.size, &read);
            if (!rc) rc = mulib_rootfs_put (&copy, p->path, data.data, read, 1);
            mulib_ct_blob_free (&data);
            break;
          }
        default: rc = MULIB_VFS_EINVAL; break;
        }
      if (!rc) rc = mulib_rootfs_usage (&copy, &bytes, NULL, NULL);
      if (!rc && bytes > s->max_result_bytes) rc = MULIB_VFS_ELIMIT;
      if (rc && failed) *failed = i;
    }
  if (!rc) rc = mulib_rootfs_usage (&copy, &bytes, NULL, NULL);
  if (!rc && bytes > s->max_result_bytes) rc = MULIB_VFS_ELIMIT;
  if (rc)
    {
      if (detail) *detail = rc;
      mulib_rootfs_free (&copy);
      return rc == MULIB_VFS_ELIMIT ? MULIB_CT_ELIMIT
             : rc == MULIB_VFS_ENOMEM ? MULIB_CT_ENOMEM : MULIB_CT_EIO;
    }
  mulib_rootfs_free (root); *root = copy;
  return 0;
}
#endif
