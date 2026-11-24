/** @file mulib-sshpacket.h
 * @brief SSH binary framing for cleartext and length-authenticated AEAD packets.
 * Define MULIB_IMPL_SSHPACKET and ENDIAN.
 */
#ifndef MULIB_SSHPACKET_H
#define MULIB_SSHPACKET_H
#include "mulib-sshwire.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {
  MULIB_SSH_PAYLOAD_MAX = 32768 + 13, MULIB_SSH_BODY_MAX = MULIB_SSH_PAYLOAD_MAX + 256,
  MULIB_SSH_FRAME_MAX = 4 + MULIB_SSH_BODY_MAX + 16
};
typedef int (*mulib_ssh_random_fn) (void *, void *, size_t);

/* BODY = padding length, payload, padding. Clear frames align including the
 * uint32 length at 8 bytes. GCM aligns the body alone at 16 bytes. */
size_t mulib_sshpacket_body_size (size_t payload_length, int gcm);
/* Caller supplies disjoint buffers. Returns 1/0 and sets used=0 on failure. */
int mulib_sshpacket_write (const void *, size_t, int gcm, void *, size_t,
                           size_t *used, mulib_ssh_random_fn, void *);
/* Inspect the clear length, accounting for the GCM tag when enabled.
 * 1 complete, 0 incomplete, -1 invalid. Outputs committed only on 1. */
int mulib_sshpacket_peek (const void *, size_t, int gcm, size_t *body_length, size_t *frame_length);
/* Validate a complete plaintext body and return a borrowed payload (1/0). */
int mulib_sshpacket_payload (const void *, size_t, mulib_ssh_slice_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SSHPACKET) && !defined(MULIB_SSHPACKET_IMPLEMENTED)
#define MULIB_SSHPACKET_IMPLEMENTED
#include <string.h>

size_t
mulib_sshpacket_body_size (size_t length, int gcm)
{
  if (!length || length > MULIB_SSH_PAYLOAD_MAX) return 0;
  size_t alignment = gcm ? 16 : 8;
  size_t offset = gcm ? 0 : 4;
  size_t padding = alignment - ((offset + 1 + length) % alignment);
  if (padding < 4) padding += alignment;
  return 1 + length + padding;
}

int
mulib_sshpacket_write (const void *payload, size_t length, int gcm,
                       void *out, size_t cap, size_t *used,
                       mulib_ssh_random_fn random, void *context)
{
  unsigned char *p = (unsigned char *)out;
  size_t body = mulib_sshpacket_body_size (length, gcm);
  if (used) *used = 0;
  if (!body || !payload || !p || !used || !random || cap < body + 4) return 0;
  size_t padding = body - length - 1;
  if (random (context, p + 5 + length, padding) != 1) return 0;
  mulib_endian_store_be32 (p, (uint32_t)body);
  p[4] = (unsigned char)padding;
  memcpy (p + 5, payload, length);
  *used = body + 4;
  return 1;
}

int
mulib_sshpacket_peek (const void *in, size_t size, int gcm,
                      size_t *body_length, size_t *frame_length)
{
  const unsigned char *p = (const unsigned char *)in;
  if ((!p && size) || !body_length || !frame_length) return -1;
  if (size < 4) return 0;
  uint32_t body = mulib_endian_load_be32 (p);
  if (body < 6 || body > MULIB_SSH_BODY_MAX
      || (gcm ? body % 16 : (body + 4) % 8)) return -1;
  size_t frame = 4 + (size_t)body + (gcm ? 16 : 0);
  if (size < frame) return 0;
  *body_length = body;
  *frame_length = frame;
  return 1;
}

int
mulib_sshpacket_payload (const void *body, size_t size, mulib_ssh_slice_t *out)
{
  const unsigned char *p = (const unsigned char *)body;
  if (!p || !out || size < 6 || size > MULIB_SSH_BODY_MAX
      || p[0] < 4 || (size_t)p[0] + 1 >= size) return 0;
  size_t length = size - p[0] - 1;
  if (length > MULIB_SSH_PAYLOAD_MAX) return 0;
  out->data = p + 1;
  out->length = length;
  return 1;
}
#endif
