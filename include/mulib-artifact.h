/**
 * @file mulib-artifact.h
 * @brief Immutable, reference-counted values, references and application objects.
 * Define MULIB_IMPL_ARTIFACT once. No build-system or filesystem assumptions.
 */
#ifndef MULIB_ARTIFACT_H
#define MULIB_ARTIFACT_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_artifact mulib_artifact_t;
typedef enum mulib_artifact_kind {
  MULIB_ARTIFACT_BYTES, MULIB_ARTIFACT_REFERENCE, MULIB_ARTIFACT_OBJECT
} mulib_artifact_kind_t;
/** Optional object destructor, called exactly once after the last release. */
typedef void (*mulib_artifact_destroy_fn) (void *object);
typedef struct mulib_artifact_desc {
  const char *id;                 /**< Required nonempty application identity. */
  const char *type;               /**< Required nonempty application type. */
  const char *media_type;         /**< Optional MIME type; no interpretation. */
  const char *locator;            /**< Optional URI; required for REFERENCE. */
  mulib_artifact_kind_t kind;
  const void *data;               /**< BYTES only; copied, including embedded NULs. */
  size_t length;
  void *object;                  /**< OBJECT only; borrowed if destroy is NULL. */
  mulib_artifact_destroy_fn destroy;
} mulib_artifact_desc_t;
/** Copy strings/bytes. Adopt an object only on success. NULL on invalid/OOM. */
mulib_artifact_t *mulib_artifact_create (const mulib_artifact_desc_t *desc);
/** Add a reference; NULL on NULL or reference-count overflow. Not atomic. */
mulib_artifact_t *mulib_artifact_retain (mulib_artifact_t *artifact);
/** Release one reference; NULL safe. External synchronization is required. */
void mulib_artifact_release (mulib_artifact_t *artifact);
/** Borrow the immutable descriptor until the last release. NULL safe. */
const mulib_artifact_desc_t *mulib_artifact_view (const mulib_artifact_t *artifact);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_ARTIFACT) && !defined(MULIB_ARTIFACT_IMPLEMENTED)
#define MULIB_ARTIFACT_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
struct mulib_artifact {
  mulib_artifact_desc_t value;
  size_t references;
};
static char *
mulib_artifact_copy_ (const char *s)
{
  size_t n;
  char *p;
  if (!s) return NULL;
  n = strlen (s);
  if (n == SIZE_MAX) return NULL;
  p = (char *)malloc (n + 1);
  if (p) memcpy (p, s, n + 1);
  return p;
}
mulib_artifact_t *
mulib_artifact_create (const mulib_artifact_desc_t *d)
{
  mulib_artifact_t *a;
  if (!d || !d->id || !*d->id || !d->type || !*d->type
      || d->kind < MULIB_ARTIFACT_BYTES || d->kind > MULIB_ARTIFACT_OBJECT
      || (!d->data && d->length) || d->length > (size_t)PTRDIFF_MAX
      || (d->kind != MULIB_ARTIFACT_BYTES && (d->data || d->length))
      || (d->kind != MULIB_ARTIFACT_OBJECT && (d->object || d->destroy))
      || (d->kind == MULIB_ARTIFACT_OBJECT && !d->object)
      || (d->kind == MULIB_ARTIFACT_REFERENCE && (!d->locator || !*d->locator)))
    return NULL;
  a = (mulib_artifact_t *)calloc (1, sizeof (*a));
  if (!a) return NULL;
  a->references = 1;
  a->value.kind = d->kind;
  a->value.id = mulib_artifact_copy_ (d->id);
  a->value.type = mulib_artifact_copy_ (d->type);
  a->value.media_type = mulib_artifact_copy_ (d->media_type);
  a->value.locator = mulib_artifact_copy_ (d->locator);
  if (d->length) {
    void *bytes = malloc (d->length);
    if (bytes) memcpy (bytes, d->data, d->length);
    a->value.data = bytes;
  }
  if (!a->value.id || !a->value.type || (d->media_type && !a->value.media_type)
      || (d->locator && !a->value.locator) || (d->length && !a->value.data)) {
    mulib_artifact_release (a);
    return NULL;
  }
  a->value.length = d->length;
  a->value.object = d->object;
  a->value.destroy = d->destroy;
  return a;
}
mulib_artifact_t *
mulib_artifact_retain (mulib_artifact_t *a)
{
  if (!a || a->references == SIZE_MAX) return NULL;
  ++a->references;
  return a;
}
void
mulib_artifact_release (mulib_artifact_t *a)
{
  if (!a || --a->references) return;
  if (a->value.destroy) a->value.destroy (a->value.object);
  free ((void *)a->value.id);
  free ((void *)a->value.type);
  free ((void *)a->value.media_type);
  free ((void *)a->value.locator);
  free ((void *)a->value.data);
  free (a);
}
const mulib_artifact_desc_t *
mulib_artifact_view (const mulib_artifact_t *a)
{
  return a ? &a->value : NULL;
}
#endif
