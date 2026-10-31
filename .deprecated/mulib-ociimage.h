/** @file mulib-ociimage.h
 * @brief OCI image-manifest v1 JSON descriptor validation, not blob transport.
 * Define MULIB_IMPL_OCIIMAGE plus JSON and its dependencies. Digests not
 * authenticated. */
#ifndef MULIB_OCIIMAGE_H
#define MULIB_OCIIMAGE_H
#include "mulib-json.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Validate exactly sha256: followed by 64 lowercase hex digits. */
  int mulib_ociimage_digest (const char *digest);
  /** @brief Require object descriptor with mediaType string, sha256 digest and
   * integral size in [0,2^53-1]. Descriptor extensions are retained/ignored.
   */
  int mulib_ociimage_descriptor (const mulib_json_t *descriptor);
  /** @brief Require schemaVersion=2, OCI manifest mediaType, config descriptor
   * and layers array (<=4096 validated descriptors). Unknown extensions
   * retained. Does not verify media-type-specific blobs, hashes, signatures or
   * platform config. */
  int mulib_ociimage_validate (const mulib_json_t *manifest);
  /** @brief Parse/validate; caller owns returned JSON. NULL includes profile
   * errors. */
  mulib_json_t *mulib_ociimage_parse (const char *json, size_t length,
                                      mulib_json_error_t *error);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OCIIMAGE) && !defined(MULIB_OCIIMAGE_IMPLEMENTED)
#define MULIB_OCIIMAGE_IMPLEMENTED
#include <math.h>
#include <string.h>
int
mulib_ociimage_digest (const char *s)
{
  size_t i;
  if (!s || strlen (s) != 71 || memcmp (s, "sha256:", 7))
    return 0;
  for (i = 7; i < 71; ++i)
    if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f')))
      return 0;
  return 1;
}
int
mulib_ociimage_descriptor (const mulib_json_t *d)
{
  const char *s;
  size_t n;
  const mulib_json_t *v;
  double size;
  if (mulib_json_typeof (d) != MULIB_JSON_OBJECT)
    return 0;
  s = mulib_json_text (mulib_json_get (d, "mediaType"), &n);
  if (!s || !n || memchr (s, 0, n))
    return 0;
  s = mulib_json_text (mulib_json_get (d, "digest"), &n);
  if (!s || n != 71 || !mulib_ociimage_digest (s))
    return 0;
  v = mulib_json_get (d, "size");
  if (mulib_json_typeof (v) != MULIB_JSON_NUMBER)
    return 0;
  size = mulib_json_numeric (v);
  return isfinite (size) && size >= 0 && size <= 9007199254740991.0
         && floor (size) == size;
}
int
mulib_ociimage_validate (const mulib_json_t *m)
{
  const mulib_json_t *v, *layers;
  const char *s;
  size_t i, n;
  if (mulib_json_typeof (m) != MULIB_JSON_OBJECT)
    return 0;
  v = mulib_json_get (m, "schemaVersion");
  if (mulib_json_typeof (v) != MULIB_JSON_NUMBER
      || mulib_json_numeric (v) != 2)
    return 0;
  s = mulib_json_text (mulib_json_get (m, "mediaType"), &n);
  if (!s || n != strlen ("application/vnd.oci.image.manifest.v1+json")
      || memcmp (s, "application/vnd.oci.image.manifest.v1+json", n))
    return 0;
  if (!mulib_ociimage_descriptor (mulib_json_get (m, "config")))
    return 0;
  layers = mulib_json_get (m, "layers");
  if (mulib_json_typeof (layers) != MULIB_JSON_ARRAY
      || mulib_json_size (layers) > 4096)
    return 0;
  for (i = 0; i < mulib_json_size (layers); ++i)
    if (!mulib_ociimage_descriptor (mulib_json_at (layers, i)))
      return 0;
  return 1;
}
mulib_json_t *
mulib_ociimage_parse (const char *s, size_t n, mulib_json_error_t *err)
{
  mulib_json_t *v = mulib_json_parse (s, n, err);
  if (!v)
    return NULL;
  if (!mulib_ociimage_validate (v))
    {
      mulib_json_destroy (v);
      if (err)
        {
          err->offset = 0;
          err->line = err->column = 1;
          err->message = "unsupported OCI manifest profile";
        }
      return NULL;
    }
  return v;
}
#endif
