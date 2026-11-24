/** @file mulib-openvpn.h
 * @brief OpenVPN data-channel v1/v2 envelopes and authenticated replay window.
 * Define MULIB_IMPL_OPENVPN once. Encryption, TLS control channel, key
 * negotiation and packet authentication are supplied by the application.
 */
#ifndef MULIB_OPENVPN_H
#define MULIB_OPENVPN_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_openvpn_packet
  {
    unsigned opcode, key_id;
    uint32_t peer_id;
    const unsigned char *payload;
    size_t length;
  } mulib_openvpn_packet_t;
  typedef struct mulib_openvpn_replay
  {
    uint64_t seen;
    uint32_t highest;
  } mulib_openvpn_replay_t;
  /** @brief Decode UDP envelope opcodes 6 (DATA_V1) or 9 (DATA_V2). 1 success,
   * 0 malformed, -1 unsupported opcode. Borrow payload without authentication.
   */
  int mulib_openvpn_decode (const void *, size_t, mulib_openvpn_packet_t *);
  /** @brief Encode data envelope, key_id 0..7, peer_id 24 bits for v2; v1
   * requires peer_id=0. 1 success/0 failure; used reset on failure. */
  int mulib_openvpn_encode (const mulib_openvpn_packet_t *, void *, size_t,
                            size_t *used);
  /** @brief Accept authenticated packet id in a 64-packet sliding window.
   * Zero-initialize per key/session. 1 new, 0 duplicate/too old/zero. Call
   * ONLY after MAC/AEAD verification; IDs cannot wrap (rekey first). */
  int mulib_openvpn_replay_accept (mulib_openvpn_replay_t *, uint32_t id);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OPENVPN) && !defined(MULIB_OPENVPN_IMPLEMENTED)
#define MULIB_OPENVPN_IMPLEMENTED
#include <string.h>
int
mulib_openvpn_decode (const void *data, size_t n, mulib_openvpn_packet_t *out)
{
  const unsigned char *p = data;
  mulib_openvpn_packet_t v;
  size_t h;
  if (!p || !n || !out)
    return 0;
  v.opcode = p[0] >> 3;
  v.key_id = p[0] & 7;
  v.peer_id = 0;
  if (v.opcode != 6 && v.opcode != 9)
    return -1;
  h = v.opcode == 6 ? 1 : 4;
  if (n <= h)
    return 0;
  if (h == 4)
    v.peer_id = (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
  v.payload = p + h;
  v.length = n - h;
  *out = v;
  return 1;
}
int
mulib_openvpn_encode (const mulib_openvpn_packet_t *v, void *output,
                      size_t cap, size_t *used)
{
  unsigned char *p = output;
  size_t h;
  if (used)
    *used = 0;
  if (!v || !p || !used || !v->payload || !v->length || v->key_id > 7
      || v->peer_id > 0xffffff || (v->opcode != 6 && v->opcode != 9)
      || (v->opcode == 6 && v->peer_id))
    return 0;
  h = v->opcode == 6 ? 1 : 4;
  if (cap < h || v->length > cap - h)
    return 0;
  p[0] = (unsigned char)((v->opcode << 3) | v->key_id);
  if (h == 4)
    {
      p[1] = (unsigned char)(v->peer_id >> 16);
      p[2] = (unsigned char)(v->peer_id >> 8);
      p[3] = (unsigned char)v->peer_id;
    }
  memcpy (p + h, v->payload, v->length);
  *used = h + v->length;
  return 1;
}
int
mulib_openvpn_replay_accept (mulib_openvpn_replay_t *s, uint32_t id)
{
  uint32_t delta;
  uint64_t bit;
  if (!s || !id)
    return 0;
  if (id > s->highest)
    {
      delta = id - s->highest;
      s->seen = delta >= 64 ? 1 : (s->seen << delta) | 1;
      s->highest = id;
      return 1;
    }
  delta = s->highest - id;
  if (delta >= 64)
    return 0;
  bit = UINT64_C (1) << delta;
  if (s->seen & bit)
    return 0;
  s->seen |= bit;
  return 1;
}
#endif
