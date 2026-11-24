/** @file mulib-containerserverd.h
 * @brief Single-container request dispatcher; no listener, threads or implicit
 * daemon. Define MULIB_IMPL_CONTAINERSERVERD plus runtime/metadata/protocol
 * dependencies. Caller MUST authenticate/authorize transport before dispatch.
 */
#ifndef MULIB_CONTAINERSERVERD_H
#define MULIB_CONTAINERSERVERD_H
#include "mulib-container2daemonproto.h"
#include "mulib-ocicontainermetadata.h"
#include "mulib-ocicontainerruntime.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Handle exactly one complete START/STOP/DELETE/STATUS frame for
   * runtime ID. Response opcode=request|128, ID echoed, payload state JSON. 0
   * success, -1 malformed/unsupported/ID mismatch, -2 allocation, other
   * backend errors. Allocation precedes backend action; failures leave
   * response bytes/state unchanged. STOP invokes backend with code zero;
   * actual process supervision belongs to backend. */
  int mulib_containerserverd_dispatch (mulib_ocicontainerruntime_t *runtime,
                                       const void *request, size_t size,
                                       mulib_buffer_t *response);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINERSERVERD)                                      \
    && !defined(MULIB_CONTAINERSERVERD_IMPLEMENTED)
#define MULIB_CONTAINERSERVERD_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
int
mulib_containerserverd_dispatch (mulib_ocicontainerruntime_t *r, const void *p,
                                 size_t n, mulib_buffer_t *out)
{
  mulib_container2daemonproto_frame_t f;
  mulib_ocicontainer_t desired;
  size_t used, oldlen;
  int next = 0, rc;
  mulib_json_t *json;
  char *text;
  if (!r || !out || !memchr (r->container.id, 0, sizeof (r->container.id))
      || mulib_container2daemonproto_decode (p, n, &f, &used) != 1 || used != n
      || f.opcode < 1 || f.opcode > 4 || f.size != strlen (r->container.id)
      || memcmp (f.payload, r->container.id, f.size))
    return -1;
  desired = r->container;
  if (f.opcode != 4)
    {
      next = f.opcode == 1   ? MULIB_OCICONTAINER_RUNNING
             : f.opcode == 2 ? MULIB_OCICONTAINER_STOPPED
                             : MULIB_OCICONTAINER_DELETED;
      if (!mulib_ocicontainer_transition (&desired, next, 0))
        return -1;
    }
  json = mulib_ocicontainermetadata_json (&desired);
  if (!json)
    return -2;
  text = mulib_json_stringify (json, 0);
  mulib_json_destroy (json);
  if (!text)
    return -2;
  oldlen = out->length;
  rc = mulib_container2daemonproto_encode (out, f.opcode | 128, f.request_id,
                                           text, strlen (text))
           ? 0
           : -2;
  free (text);
  if (rc)
    return rc;
  if (next)
    {
      rc = mulib_ocicontainerruntime_apply (r, next, 0);
      if (rc)
        out->length = oldlen;
    }
  return rc;
}
#endif
