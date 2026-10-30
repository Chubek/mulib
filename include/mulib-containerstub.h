/** @file mulib-containerstub.h
 * @brief Client-side lifecycle-request encoder, with caller-supplied
 * transport. Define MULIB_IMPL_CONTAINERSTUB plus OCICONTAINER and
 * CONTAINER2DAEMONPROTO deps. */
#ifndef MULIB_CONTAINERSTUB_H
#define MULIB_CONTAINERSTUB_H
#include "mulib-container2daemonproto.h"
#include "mulib-ocicontainer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Encode START/STOP/DELETE/STATUS with ID bytes as payload. 1
   * success. No NUL terminator on wire. STOP requests the backend's code-zero
   * stop action. */
  int mulib_containerstub_request (mulib_buffer_t *out, unsigned opcode,
                                   uint64_t request_id,
                                   const char *container_id);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINERSTUB)                                         \
    && !defined(MULIB_CONTAINERSTUB_IMPLEMENTED)
#define MULIB_CONTAINERSTUB_IMPLEMENTED
#include <string.h>
int
mulib_containerstub_request (mulib_buffer_t *out, unsigned op, uint64_t id,
                             const char *name)
{
  mulib_ocicontainer_t c;
  return op >= 1 && op <= 4 && mulib_ocicontainer_create (&c, name)
         && mulib_container2daemonproto_encode (out, op, id, name,
                                                strlen (name));
}
#endif
