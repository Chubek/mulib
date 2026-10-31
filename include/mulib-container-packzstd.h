/** @file mulib-container-packzstd.h
 * @brief Standard Zstandard raw/RLE packaging for validated container images.
 * Define MULIB_IMPL_CONTAINER_PACKZSTD and IMGFORMAT dependencies. */
#ifndef MULIB_CONTAINER_PACKZSTD_H
#define MULIB_CONTAINER_PACKZSTD_H
#include "mulib-imgformat.h"
#include "include-mulib-zstd.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_container_packzstd_info
{
  size_t image_bytes, packed_bytes;
  uint32_t image_checksum;
  mulib_imgformat_arch architecture;
} mulib_container_packzstd_info;
/** @brief Validate an image, emit a standard raw/RLE frame, atomically replace out.
 * max_image and max_packed independently bound input and output. */
int mulib_container_packzstd_pack (const void *image, size_t length,
    size_t max_image, size_t max_packed, mulib_ct_blob *out,
    mulib_container_packzstd_info *info);
/** @brief Decode and validate image before publishing output. Entropy-compressed
 * blocks are explicit ENOTSUP; no external library is loaded implicitly. */
int mulib_container_packzstd_unpack (const void *packed, size_t length,
    size_t max_packed, size_t max_image, mulib_ct_blob *out,
    mulib_container_packzstd_info *info);
/** @brief Optional caller codec for full Zstandard support. It must replace an
 * initialized empty output blob using CT allocation, enforce output_limit, and
 * return zero or negative CT error. User/context remain borrowed. */
typedef int (*mulib_container_packzstd_codec) (void *user, const void *input,
    size_t length, size_t output_limit, mulib_ct_blob *output);
/** @brief Decode using a supplied codec, then apply identical envelope validation.
 * A successful callback exceeding output_limit is rejected and released. */
int mulib_container_packzstd_unpack_with (const void *packed, size_t length,
    size_t max_packed, size_t max_image, mulib_container_packzstd_codec codec,
    void *user, mulib_ct_blob *out, mulib_container_packzstd_info *info);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINER_PACKZSTD) && !defined(MULIB_CONTAINER_PACKZSTD_IMPLEMENTED)
#define MULIB_CONTAINER_PACKZSTD_IMPLEMENTED
static void
mulib_container_packzstd_info_ (mulib_container_packzstd_info *info,
    const mulib_imgformat_view *v, size_t image, size_t packed)
{
  if (!info) return;
  info->image_bytes = image; info->packed_bytes = packed;
  info->image_checksum = v->checksum; info->architecture = v->architecture;
}
int
mulib_container_packzstd_pack (const void *image, size_t n, size_t image_limit,
    size_t packed_limit, mulib_ct_blob *out, mulib_container_packzstd_info *info)
{
  mulib_imgformat_view view;
  int rc;
  if (!out) return MULIB_CT_EINVAL;
  rc = mulib_imgformat_open (image, n, image_limit, &view);
  if (rc) return rc;
  rc = mulib_zstd_raw_encode (image, n, out, packed_limit);
  if (!rc) mulib_container_packzstd_info_ (info, &view, n, out->length);
  return rc;
}
static int mulib_container_packzstd_decode_ (void *user, const void *p, size_t n,
                                            size_t limit, mulib_ct_blob *out)
{ (void)user; return mulib_zstd_raw_decode (p, n, out, limit); }
int
mulib_container_packzstd_unpack_with (const void *p, size_t n, size_t packed_limit,
    size_t image_limit, mulib_container_packzstd_codec codec, void *user,
    mulib_ct_blob *out, mulib_container_packzstd_info *info)
{
  mulib_ct_blob image = { NULL, 0, 0 };
  mulib_imgformat_view view;
  int rc;
  if (!p || !out || !codec) return MULIB_CT_EINVAL;
  if (n > packed_limit) return MULIB_CT_ELIMIT;
  rc = codec (user, p, n, image_limit, &image);
  if (rc > 0) rc = MULIB_CT_EIO;
  if (!rc && (image.length > image_limit || image.length > image.capacity
      || (!image.data && image.length))) rc = MULIB_CT_ELIMIT;
  if (!rc) rc = mulib_imgformat_open (image.data, image.length, image_limit, &view);
  if (rc) { mulib_ct_blob_free (&image); return rc; }
  mulib_container_packzstd_info_ (info, &view, image.length, n);
  mulib_ct_blob_free (out); *out = image;
  return 0;
}
int
mulib_container_packzstd_unpack (const void *p, size_t n, size_t packed_limit,
    size_t image_limit, mulib_ct_blob *out, mulib_container_packzstd_info *info)
{
  return mulib_container_packzstd_unpack_with (p, n, packed_limit, image_limit,
      mulib_container_packzstd_decode_, NULL, out, info);
}
#endif
