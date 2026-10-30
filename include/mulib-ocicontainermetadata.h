/** @file mulib-ocicontainermetadata.h
 * @brief OCI-style state JSON projection from the lifecycle model.
 * Define MULIB_IMPL_OCICONTAINERMETADATA plus OCICONTAINER and JSON
 * dependencies. */
#ifndef MULIB_OCICONTAINERMETADATA_H
#define MULIB_OCICONTAINERMETADATA_H
#include "mulib-json.h"
#include "mulib-ocicontainer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Return malloc-owned JSON object with ociVersion, id, status and
   * exitCode. Deleted state is a library extension. No PID/bundle path is
   * fabricated. NULL invalid/OOM; caller destroys JSON. Not a full OCI state
   * schema. */
  mulib_json_t *
  mulib_ocicontainermetadata_json (const mulib_ocicontainer_t *container);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OCICONTAINERMETADATA)                                  \
    && !defined(MULIB_OCICONTAINERMETADATA_IMPLEMENTED)
#define MULIB_OCICONTAINERMETADATA_IMPLEMENTED
#include <string.h>
static int
mulib_ocicontainermetadata_set_ (mulib_json_t *o, const char *key,
                                 mulib_json_t *v)
{
  if (!v)
    return 0;
  if (mulib_json_set (o, key, v))
    return 1;
  mulib_json_destroy (v);
  return 0;
}
mulib_json_t *
mulib_ocicontainermetadata_json (const mulib_ocicontainer_t *c)
{
  mulib_json_t *v;
  const char *status;
  mulib_ocicontainer_t checked;
  if (!c || !memchr (c->id, 0, sizeof (c->id))
      || !mulib_ocicontainer_create (&checked, c->id))
    return NULL;
  switch (c->state)
    {
    case 1:
      status = "created";
      break;
    case 2:
      status = "running";
      break;
    case 3:
      status = "stopped";
      break;
    case 4:
      status = "deleted";
      break;
    default:
      return NULL;
    }
  v = mulib_json_new (MULIB_JSON_OBJECT);
  if (!v)
    return NULL;
  if (!mulib_ocicontainermetadata_set_ (v, "ociVersion",
                                        mulib_json_string ("1.0.2"))
      || !mulib_ocicontainermetadata_set_ (v, "id", mulib_json_string (c->id))
      || !mulib_ocicontainermetadata_set_ (v, "status",
                                           mulib_json_string (status))
      || !mulib_ocicontainermetadata_set_ (v, "exitCode",
                                           mulib_json_number (c->exit_code)))
    {
      mulib_json_destroy (v);
      return NULL;
    }
  return v;
}
#endif
