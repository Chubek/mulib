/** @file mulib-image2container.h
 * @brief Atomic image materialization into owned container metadata and rootfs.
 * Define MULIB_IMPL_IMAGE2CONTAINER and explicit dependency switches. */
#ifndef MULIB_IMAGE2CONTAINER_H
#define MULIB_IMAGE2CONTAINER_H
#include "mulib-imgformat.h"
#include "mulib-rootfs.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_image2container_limits
{
  size_t image_bytes, nodes, file_bytes, total_file_bytes;
} mulib_image2container_limits;
/** @brief Owns both objects; neither refers to the source image after open. */
typedef struct mulib_image2container
{
  mulib_container container;
  mulib_rootfs_t rootfs;
  mulib_imgformat_arch architecture;
  uint32_t image_checksum;
} mulib_image2container;
/** @brief Conservative defaults matching the image builder. */
mulib_image2container_limits mulib_image2container_default_limits (void);
/** @brief Validate and import into uninitialized output. Optional replacement_id
 * clones the image under a new identity. Returns common CT errors; detail
 * receives an underlying VFS error where applicable. */
int mulib_image2container_open (const void *image, size_t length,
                                const char *replacement_id,
                                const mulib_image2container_limits *limits,
                                mulib_image2container *out, int *detail);
/** @brief Clone a materialized image into uninitialized output with a new ID. */
int mulib_image2container_clone (const mulib_image2container *source,
                                 const char *id, mulib_image2container *out);
/** @brief Release metadata and filesystem after any runtime using them is closed. */
void mulib_image2container_free (mulib_image2container *image);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_IMAGE2CONTAINER) && !defined(MULIB_IMAGE2CONTAINER_IMPLEMENTED)
#define MULIB_IMAGE2CONTAINER_IMPLEMENTED
mulib_image2container_limits
mulib_image2container_default_limits (void)
{
  mulib_image2container_limits l = { 128U * 1024U * 1024U, 4096,
                                    16U * 1024U * 1024U, 64U * 1024U * 1024U };
  return l;
}
void
mulib_image2container_free (mulib_image2container *image)
{
  if (!image) return;
  mulib_container_free (&image->container); mulib_rootfs_free (&image->rootfs);
  memset (image, 0, sizeof (*image));
}
int
mulib_image2container_open (const void *data, size_t n, const char *id,
                            const mulib_image2container_limits *limits,
                            mulib_image2container *out, int *detail)
{
  mulib_image2container_limits defaults = mulib_image2container_default_limits ();
  mulib_image2container next;
  mulib_container_spec spec;
  mulib_imgformat_view view;
  size_t bytes;
  int rc;
  if (detail) *detail = 0;
  if (!out || (id && !mulib_ct_id_valid (id))) return MULIB_CT_EINVAL;
  if (!limits) limits = &defaults;
  memset (&next, 0, sizeof (next));
  rc = mulib_imgformat_open (data, n, limits->image_bytes, &view);
  if (rc) return rc;
  rc = mulib_imgformat_spec_decode (view.config, view.config_length, &spec);
  if (rc) return rc;
  if (id) (void)mulib_ct_text_copy (spec.id, sizeof (spec.id), id);
  rc = mulib_container_init (&next.container, &spec);
  mulib_container_spec_free (&spec);
  if (rc) return rc;
  rc = mulib_rootfs_import_limited (view.rootfs, view.rootfs_length, limits->nodes,
      limits->file_bytes, limits->total_file_bytes, &next.rootfs);
  if (rc)
    {
      if (detail) *detail = rc;
      rc = rc == MULIB_VFS_ELIMIT ? MULIB_CT_ELIMIT
           : rc == MULIB_VFS_ENOMEM ? MULIB_CT_ENOMEM : MULIB_CT_EFORMAT;
    }
  if (!rc)
    {
      if (mulib_rootfs_usage (&next.rootfs, &bytes, NULL, NULL)
          || bytes > limits->total_file_bytes) rc = MULIB_CT_ELIMIT;
    }
  if (rc) { mulib_image2container_free (&next); return rc; }
  next.architecture = view.architecture; next.image_checksum = view.checksum;
  *out = next;
  return 0;
}
int
mulib_image2container_clone (const mulib_image2container *source, const char *id,
                             mulib_image2container *out)
{
  mulib_image2container next;
  mulib_container_spec spec;
  int rc;
  if (!source || !out || source == out || !mulib_ct_id_valid (id)) return MULIB_CT_EINVAL;
  memset (&next, 0, sizeof (next));
  rc = mulib_container_spec_clone (&source->container.spec, &spec);
  if (rc) return rc;
  (void)mulib_ct_text_copy (spec.id, sizeof (spec.id), id);
  rc = mulib_container_init (&next.container, &spec);
  mulib_container_spec_free (&spec);
  if (!rc)
    {
      int native = mulib_rootfs_clone (&source->rootfs, &next.rootfs);
      if (native) rc = native == MULIB_VFS_ENOMEM ? MULIB_CT_ENOMEM
                       : native == MULIB_VFS_ELIMIT ? MULIB_CT_ELIMIT : MULIB_CT_EINVAL;
    }
  if (rc) { mulib_image2container_free (&next); return rc; }
  next.architecture = source->architecture; next.image_checksum = source->image_checksum;
  *out = next;
  return 0;
}
#endif
