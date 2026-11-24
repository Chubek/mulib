/** @file mulib-imgbuildspec.h
 * @brief Reusable owned image recipes combining process and filesystem specs.
 * Define MULIB_IMPL_IMGBUILDSPEC and explicit dependency switches. */
#ifndef MULIB_IMGBUILDSPEC_H
#define MULIB_IMGBUILDSPEC_H
#include "mulib-imgformat.h"
#include "mulib-rootfs-buildspec.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_imgbuildspec
{
  mulib_container_spec process;
  mulib_rootfs_buildspec filesystem;
  mulib_imgformat_arch architecture;
  size_t max_nodes, max_file_bytes, max_image_bytes;
} mulib_imgbuildspec;
/** @brief Defaults: 4096 nodes/steps, 16 MiB files, 64 MiB recipes/results,
 * 128 MiB images. Limits can be lowered before adding data. */
int mulib_imgbuildspec_init (mulib_imgbuildspec *spec, const char *id);
/** @brief Free both owned sub-specifications. */
void mulib_imgbuildspec_free (mulib_imgbuildspec *spec);
/** @brief Validate static quotas, architecture, process metadata and step sizes. */
int mulib_imgbuildspec_validate (const mulib_imgbuildspec *spec);
/** @brief Deep-copy into uninitialized output; leave output unchanged on failure. */
int mulib_imgbuildspec_clone (const mulib_imgbuildspec *spec, mulib_imgbuildspec *out);
/** @brief Convenience copied file insertion, creating parents during build. */
int mulib_imgbuildspec_file (mulib_imgbuildspec *spec, const char *path,
                            const void *bytes, size_t length);
/** @brief Convenience directory insertion; duplicate directory steps are harmless. */
int mulib_imgbuildspec_directory (mulib_imgbuildspec *spec, const char *path);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_IMGBUILDSPEC) && !defined(MULIB_IMGBUILDSPEC_IMPLEMENTED)
#define MULIB_IMGBUILDSPEC_IMPLEMENTED
int
mulib_imgbuildspec_init (mulib_imgbuildspec *s, const char *id)
{
  mulib_imgbuildspec next;
  int rc;
  if (!s) return MULIB_CT_EINVAL;
  memset (&next, 0, sizeof (next));
  rc = mulib_container_spec_init (&next.process, id);
  if (rc) return rc;
  rc = mulib_rootfs_buildspec_init (&next.filesystem, 4096, 64U * 1024U * 1024U,
                                   64U * 1024U * 1024U);
  if (rc) { mulib_container_spec_free (&next.process); return rc; }
  next.max_nodes = 4096; next.max_file_bytes = 16U * 1024U * 1024U;
  next.max_image_bytes = 128U * 1024U * 1024U;
  next.architecture = MULIB_IMGFORMAT_PORTABLE; *s = next;
  return 0;
}
void
mulib_imgbuildspec_free (mulib_imgbuildspec *s)
{
  if (!s) return;
  mulib_container_spec_free (&s->process);
  mulib_rootfs_buildspec_free (&s->filesystem); memset (s, 0, sizeof (*s));
}
int
mulib_imgbuildspec_validate (const mulib_imgbuildspec *s)
{
  size_t i;
  int rc;
  if (!s || !s->max_nodes || s->max_nodes > SIZE_MAX / sizeof (mulib_vfs_mem_node)
      || s->max_image_bytes < 64 || s->architecture < 0
      || s->architecture > MULIB_IMGFORMAT_QVM
      || !s->filesystem.max_steps || s->filesystem.count > s->filesystem.max_steps
      || s->filesystem.bytes > s->filesystem.max_bytes) return MULIB_CT_EINVAL;
  rc = mulib_container_spec_validate (&s->process);
  if (rc) return rc;
  for (i = 0; i < s->filesystem.count; ++i)
    if (s->filesystem.steps[i].bytes.length > s->max_file_bytes) return MULIB_CT_ELIMIT;
  return 0;
}
int
mulib_imgbuildspec_clone (const mulib_imgbuildspec *s, mulib_imgbuildspec *out)
{
  mulib_imgbuildspec copy;
  int rc;
  if (!out || out == s) return MULIB_CT_EINVAL;
  rc = mulib_imgbuildspec_validate (s);
  if (rc) return rc;
  copy = *s; memset (&copy.process, 0, sizeof (copy.process));
  memset (&copy.filesystem, 0, sizeof (copy.filesystem));
  rc = mulib_container_spec_clone (&s->process, &copy.process);
  if (!rc) rc = mulib_rootfs_buildspec_clone (&s->filesystem, &copy.filesystem);
  if (rc) { mulib_imgbuildspec_free (&copy); return rc; }
  *out = copy;
  return 0;
}
int
mulib_imgbuildspec_file (mulib_imgbuildspec *s, const char *path,
                        const void *data, size_t n)
{
  if (!s) return MULIB_CT_EINVAL;
  if (n > s->max_file_bytes) return MULIB_CT_ELIMIT;
  return mulib_rootfs_buildspec_add (&s->filesystem, MULIB_ROOTFS_BUILD_FILE,
                                    path, NULL, data, n, 0);
}
int
mulib_imgbuildspec_directory (mulib_imgbuildspec *s, const char *path)
{
  if (!s) return MULIB_CT_EINVAL;
  return mulib_rootfs_buildspec_add (&s->filesystem, MULIB_ROOTFS_BUILD_DIRECTORY,
                                    path, NULL, NULL, 0, 0);
}
#endif
