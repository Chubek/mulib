/** @file mulib-container2daemonproto.h
 * @brief Bounded versioned CRC framing for caller-transported container
 * messages. Define MULIB_IMPL_CONTAINER2DAEMONPROTO plus BUFFER, BYCUTILS and
 * CRC32. CRC detects accidents, not malicious modification; authentication is
 * external. */
#ifndef MULIB_CONTAINER2DAEMONPROTO_H
#define MULIB_CONTAINER2DAEMONPROTO_H
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_CONTAINER2DAEMONPROTO_START = 1,
    MULIB_CONTAINER2DAEMONPROTO_STOP = 2,
    MULIB_CONTAINER2DAEMONPROTO_DELETE = 3,
    MULIB_CONTAINER2DAEMONPROTO_STATUS = 4,
    MULIB_CONTAINER2DAEMONPROTO_IPC = 16,
    MULIB_CONTAINER2DAEMONPROTO_MAX_PAYLOAD = 1048576
  };
  typedef struct mulib_container2daemonproto_frame
  {
    unsigned opcode;
    uint64_t request_id;
    const unsigned char *payload;
    size_t size;
  } mulib_container2daemonproto_frame_t;
  /** @brief Append frame atomically; opcode 1..255, size <=1MiB. 1 success.
   * Input may alias output; protocol fields are little-endian. */
  int mulib_container2daemonproto_encode (mulib_buffer_t *out, unsigned opcode,
                                          uint64_t request_id,
                                          const void *payload, size_t size);
  /** @brief Decode one frame: 1 complete, 0 need more, -1
   * invalid/version/CRC/limit. Extra stream bytes permitted; consumed/output
   * unchanged except on success. Header is magic MCDP, version=1, opcode, two
   * reserved zero bytes, LE64 id, LE32 length; payload follows, then
   * CRC32(header+payload). */
  int
  mulib_container2daemonproto_decode (const void *data, size_t size,
                                      mulib_container2daemonproto_frame_t *out,
                                      size_t *consumed);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINER2DAEMONPROTO)                                 \
    && !defined(MULIB_CONTAINER2DAEMONPROTO_IMPLEMENTED)
#define MULIB_CONTAINER2DAEMONPROTO_IMPLEMENTED
#include "mulib-bycutils.h"
#include "mulib-crc32.h"
#include <string.h>
int
mulib_container2daemonproto_encode (mulib_buffer_t *out, unsigned op,
                                    uint64_t id, const void *p, size_t n)
{
  unsigned char h[20] = { 0 }, crc[4];
  mulib_buffer_t b = { 0 };
  int ok;
  if (!out || !op || op > 255 || (!p && n)
      || n > MULIB_CONTAINER2DAEMONPROTO_MAX_PAYLOAD)
    return 0;
  memcpy (h, "MCDP", 4);
  h[4] = 1;
  h[5] = (unsigned char)op;
  mulib_bycutils_store (h, 20, 8, 8, id);
  mulib_bycutils_store (h, 20, 16, 4, n);
  ok = mulib_buffer_append (&b, h, 20) && mulib_buffer_append (&b, p, n);
  if (ok)
    {
      mulib_bycutils_store (crc, 4, 0, 4, mulib_crc32 (b.data, b.length));
      ok = mulib_buffer_append (&b, crc, 4)
           && mulib_buffer_append (out, b.data, b.length);
    }
  mulib_buffer_free (&b);
  return ok;
}
int
mulib_container2daemonproto_decode (const void *data, size_t n,
                                    mulib_container2daemonproto_frame_t *out,
                                    size_t *used)
{
  const unsigned char *p = (const unsigned char *)data;
  uint64_t id, len, crc;
  mulib_container2daemonproto_frame_t f;
  size_t total;
  if (!out || !used || (!p && n))
    return -1;
  if (n < 20)
    return 0;
  if (memcmp (p, "MCDP", 4) || p[4] != 1 || !p[5] || p[6] || p[7])
    return -1;
  mulib_bycutils_load (p, n, 8, 8, &id);
  mulib_bycutils_load (p, n, 16, 4, &len);
  if (len > MULIB_CONTAINER2DAEMONPROTO_MAX_PAYLOAD)
    return -1;
  total = 24 + (size_t)len;
  if (n < total)
    return 0;
  if (!mulib_bycutils_load (p, n, total - 4, 4, &crc)
      || crc != mulib_crc32 (p, total - 4))
    return -1;
  f.opcode = p[5];
  f.request_id = id;
  f.payload = p + 20;
  f.size = (size_t)len;
  *out = f;
  *used = total;
  return 1;
}
#endif
