/** @file mulib-ociimagebuilder.h
 * @brief Owned OCI manifest construction from externally hashed descriptors.
 * Define MULIB_IMPL_OCIIMAGEBUILDER plus OCIIMAGE and JSON dependencies. */
#ifndef MULIB_OCIIMAGEBUILDER_H
#define MULIB_OCIIMAGEBUILDER_H
#include "mulib-ociimage.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Create a descriptor; caller owns JSON, size <= 2^53-1. No hashing.
   */
  mulib_json_t *mulib_ociimagebuilder_descriptor (const char *media_type,
                                                  const char *digest,
                                                  uint64_t size);
  /** @brief Create empty-layer manifest, copying validated config descriptor.
   * Caller retains config and owns returned manifest. NULL on invalid/OOM. */
  mulib_json_t *mulib_ociimagebuilder_create (const mulib_json_t *config);
  /** @brief Copy/add a layer to a valid manifest, preserving it on failure. 1
   * success. */
  int mulib_ociimagebuilder_layer (mulib_json_t *manifest,
                                   const mulib_json_t *descriptor);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OCIIMAGEBUILDER)                                       \
    && !defined(MULIB_OCIIMAGEBUILDER_IMPLEMENTED)
#define MULIB_OCIIMAGEBUILDER_IMPLEMENTED
#include <stdlib.h>
static int
mulib_ociimagebuilder_set_ (mulib_json_t *o, const char *k, mulib_json_t *v)
{
  if (!v)
    return 0;
  if (mulib_json_set (o, k, v))
    return 1;
  mulib_json_destroy (v);
  return 0;
}
static mulib_json_t *
mulib_ociimagebuilder_copy_ (const mulib_json_t *v)
{
  char *s = mulib_json_stringify (v, 0);
  mulib_json_t *copy;
  if (!s)
    return NULL;
  copy = mulib_json_parse (s, strlen (s), NULL);
  free (s);
  return copy;
}
mulib_json_t *
mulib_ociimagebuilder_descriptor (const char *media, const char *digest,
                                  uint64_t size)
{
  mulib_json_t *v;
  if (!media || !*media || !mulib_ociimage_digest (digest)
      || size > UINT64_C (9007199254740991))
    return NULL;
  v = mulib_json_new (MULIB_JSON_OBJECT);
  if (!v)
    return NULL;
  if (!mulib_ociimagebuilder_set_ (v, "mediaType", mulib_json_string (media))
      || !mulib_ociimagebuilder_set_ (v, "digest", mulib_json_string (digest))
      || !mulib_ociimagebuilder_set_ (v, "size",
                                      mulib_json_number ((double)size)))
    {
      mulib_json_destroy (v);
      return NULL;
    }
  return v;
}
mulib_json_t *
mulib_ociimagebuilder_create (const mulib_json_t *config)
{
  mulib_json_t *m;
  if (!mulib_ociimage_descriptor (config))
    return NULL;
  m = mulib_json_new (MULIB_JSON_OBJECT);
  if (!m)
    return NULL;
  if (!mulib_ociimagebuilder_set_ (m, "schemaVersion", mulib_json_number (2))
      || !mulib_ociimagebuilder_set_ (
          m, "mediaType",
          mulib_json_string ("application/vnd.oci.image.manifest.v1+json"))
      || !mulib_ociimagebuilder_set_ (m, "config",
                                      mulib_ociimagebuilder_copy_ (config))
      || !mulib_ociimagebuilder_set_ (m, "layers",
                                      mulib_json_new (MULIB_JSON_ARRAY)))
    {
      mulib_json_destroy (m);
      return NULL;
    }
  return m;
}
int
mulib_ociimagebuilder_layer (mulib_json_t *m, const mulib_json_t *d)
{
  mulib_json_t *v, *layers;
  if (!mulib_ociimage_validate (m) || !mulib_ociimage_descriptor (d))
    return 0;
  layers = mulib_json_get (m, "layers");
  if (mulib_json_size (layers) == 4096)
    return 0;
  v = mulib_ociimagebuilder_copy_ (d);
  if (!v)
    return 0;
  if (mulib_json_append (layers, v))
    return 1;
  mulib_json_destroy (v);
  return 0;
}
#endif
