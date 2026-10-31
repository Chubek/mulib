/** @file mulib-imgbuilder.h
 * @brief Deterministic transactional builds of checksummed rootfs images.
 * Define MULIB_IMPL_IMGBUILDER and transitive dependency switches. */
#ifndef MULIB_IMGBUILDER_H
#define MULIB_IMGBUILDER_H
#include "mulib-imgbuildspec.h"
#include "mulib-rootfsbuilder.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_imgbuilder_report
{
  size_t failed_step, files, directories, file_bytes, archive_bytes, image_bytes;
  int detail;
} mulib_imgbuilder_report;
/** @brief Apply a recipe to an optional base and build a canonical image.
 * Paths are sorted bytewise; archive ownership/timestamps are fixed to zero.
 * Output is an initialized blob, replaced only on success. Base is borrowed.
 * Report is initialized even on failure. No host filesystem access occurs. */
int mulib_imgbuilder_build (const mulib_imgbuildspec *spec,
                            const mulib_rootfs_t *base, mulib_ct_blob *out,
                            mulib_imgbuilder_report *report);
/** @brief Export a root in canonical path order with an archive size limit. */
int mulib_imgbuilder_archive (const mulib_rootfs_t *root, size_t limit,
                              mulib_ct_blob *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_IMGBUILDER) && !defined(MULIB_IMGBUILDER_IMPLEMENTED)
#define MULIB_IMGBUILDER_IMPLEMENTED
static int
mulib_imgbuilder_compare_ (const void *a, const void *b)
{
  return strcmp (((const mulib_vfs_mem_node *)a)->path,
                 ((const mulib_vfs_mem_node *)b)->path);
}
int
mulib_imgbuilder_archive (const mulib_rootfs_t *root, size_t limit, mulib_ct_blob *out)
{
  mulib_rootfs_t sorted;
  mulib_buffer_t archive;
  size_t i, bound = 1024;
  int rc;
  if (!root || !out || !root->storage.max_nodes) return MULIB_CT_EINVAL;
  for (i = 0; i < root->storage.count; ++i)
    {
      size_t n = root->storage.nodes[i].length, padding = (512 - n % 512) % 512;
      if (mulib_ct_add_size (bound, 512, &bound)
          || mulib_ct_add_size (bound, n, &bound)
          || mulib_ct_add_size (bound, padding, &bound)) return MULIB_CT_ELIMIT;
    }
  if (bound > limit || root->storage.count > SIZE_MAX / sizeof (*sorted.storage.nodes))
    return MULIB_CT_ELIMIT;
  sorted = *root; sorted.storage.nodes = NULL;
  if (root->storage.count)
    {
      sorted.storage.nodes = (mulib_vfs_mem_node *)MULIB_CT_MALLOC (
          root->storage.count * sizeof (*sorted.storage.nodes));
      if (!sorted.storage.nodes) return MULIB_CT_ENOMEM;
      memcpy (sorted.storage.nodes, root->storage.nodes,
              root->storage.count * sizeof (*sorted.storage.nodes));
      qsort (sorted.storage.nodes, sorted.storage.count,
             sizeof (*sorted.storage.nodes), mulib_imgbuilder_compare_);
    }
  mulib_buffer_init (&archive);
  rc = mulib_rootfsbuilder_export (&sorted, &archive);
  MULIB_CT_FREE (sorted.storage.nodes);
  if (!rc) rc = mulib_ct_blob_assign (out, archive.data, archive.length, limit);
  else rc = rc == MULIB_ARCHIVE_ENOMEM ? MULIB_CT_ENOMEM
             : rc == MULIB_ARCHIVE_EOVERFLOW ? MULIB_CT_ELIMIT : MULIB_CT_EFORMAT;
  mulib_buffer_free (&archive);
  return rc;
}
int
mulib_imgbuilder_build (const mulib_imgbuildspec *s, const mulib_rootfs_t *base,
                        mulib_ct_blob *out, mulib_imgbuilder_report *report)
{
  mulib_imgbuilder_report r;
  mulib_rootfs_t root;
  mulib_ct_blob archive = { NULL, 0, 0 };
  int rc;
  memset (&r, 0, sizeof (r)); r.failed_step = SIZE_MAX;
  if (report) *report = r;
  if (!out) return MULIB_CT_EINVAL;
  rc = mulib_imgbuildspec_validate (s);
  if (rc) return rc;
  rc = mulib_rootfs_init (&root, s->max_nodes, s->max_file_bytes);
  if (rc) return MULIB_CT_EINVAL;
  if (base)
    {
      size_t base_bytes;
      r.detail = mulib_rootfs_usage (base, &base_bytes, NULL, NULL);
      if (r.detail) rc = MULIB_CT_EINVAL;
      else if (base_bytes > s->filesystem.max_result_bytes) rc = MULIB_CT_ELIMIT;
      else
        {
          r.detail = mulib_rootfs_merge (&root, base, 1);
          if (r.detail) rc = r.detail == MULIB_VFS_ENOMEM ? MULIB_CT_ENOMEM
                             : r.detail == MULIB_VFS_ELIMIT ? MULIB_CT_ELIMIT : MULIB_CT_EIO;
        }
    }
  if (!rc) rc = mulib_rootfs_buildspec_apply (&s->filesystem, &root,
                                             &r.failed_step, &r.detail);
  if (!rc && mulib_rootfs_usage (&root, &r.file_bytes, &r.files, &r.directories))
    rc = MULIB_CT_ELIMIT;
  if (!rc) rc = mulib_imgbuilder_archive (&root, s->max_image_bytes, &archive);
  if (!rc)
    {
      r.archive_bytes = archive.length;
      rc = mulib_imgformat_pack (&s->process, s->architecture, archive.data,
                                 archive.length, out, s->max_image_bytes);
      if (!rc) r.image_bytes = out->length;
    }
  mulib_ct_blob_free (&archive); mulib_rootfs_free (&root);
  if (report) *report = r;
  return rc;
}
#endif
