/** @file mulib-ocicontaineripc.h
 * @brief Binary IPC channel messages using the shared container frame
 * protocol. Define MULIB_IMPL_OCICONTAINERIPC plus CONTAINER2DAEMONPROTO
 * dependencies. */
#ifndef MULIB_OCICONTAINERIPC_H
#define MULIB_OCICONTAINERIPC_H
#include "mulib-container2daemonproto.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Append IPC frame, channel carried as request_id; 1 success. */
  int mulib_ocicontaineripc_send (mulib_buffer_t *out, uint64_t channel,
                                  const void *data, size_t size);
  /** @brief Decode IPC only; 1 frame, 0 incomplete, -1 wrong
   * opcode/corrupt/invalid. Returns borrowed payload until input expires;
   * outputs unchanged on failure. */
  int mulib_ocicontaineripc_receive (const void *data, size_t size,
                                     uint64_t *channel, const void **payload,
                                     size_t *length, size_t *consumed);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OCICONTAINERIPC)                                       \
    && !defined(MULIB_OCICONTAINERIPC_IMPLEMENTED)
#define MULIB_OCICONTAINERIPC_IMPLEMENTED
int
mulib_ocicontaineripc_send (mulib_buffer_t *b, uint64_t ch, const void *p,
                            size_t n)
{
  return mulib_container2daemonproto_encode (
      b, MULIB_CONTAINER2DAEMONPROTO_IPC, ch, p, n);
}
int
mulib_ocicontaineripc_receive (const void *p, size_t n, uint64_t *ch,
                               const void **payload, size_t *len, size_t *used)
{
  mulib_container2daemonproto_frame_t f;
  size_t z;
  int rc;
  if (!ch || !payload || !len || !used)
    return -1;
  rc = mulib_container2daemonproto_decode (p, n, &f, &z);
  if (rc != 1)
    return rc;
  if (f.opcode != MULIB_CONTAINER2DAEMONPROTO_IPC)
    return -1;
  *ch = f.request_id;
  *payload = f.payload;
  *len = f.size;
  *used = z;
  return 1;
}
#endif
