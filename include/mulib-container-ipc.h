/** @file mulib-container-ipc.h
 * @brief Checksummed IPC framing with bounded incremental stream assembly.
 * Define MULIB_IMPL_CONTAINER_IPC and MULIB_IMPL_CRC32 once. */
#ifndef MULIB_CONTAINER_IPC_H
#define MULIB_CONTAINER_IPC_H
#include "include-mulib-container-core.h"
#include "mulib-crc32.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { MULIB_CONTAINER_IPC_HEADER = 32, MULIB_CONTAINER_IPC_RESPONSE = 1 };
/** @brief Borrowed message view. Status is zero for requests, 0..-14 for replies. */
typedef struct mulib_container_ipc_message
{
  uint16_t opcode, flags;
  uint64_t request_id;
  int status;
  const unsigned char *data;
  size_t length;
} mulib_container_ipc_message;
/** @brief One-message accumulator. Errors are sticky until reset. */
typedef struct mulib_container_ipc_reader
{
  mulib_ct_blob bytes;
  size_t max_payload, expected;
  int ready, error;
} mulib_container_ipc_reader;
/** @brief Replace initialized blob atomically; source may alias output. */
int mulib_container_ipc_encode (const mulib_container_ipc_message *message,
                                mulib_ct_blob *out, size_t max_payload);
/** @brief Encode into nonoverlapping caller storage without allocation. */
int mulib_container_ipc_encode_into (const mulib_container_ipc_message *message,
    void *output, size_t capacity, size_t max_payload, size_t *written);
/** @brief Decode one frame. AGAIN means incomplete; consumed is zero on failure.
 * Additional frames may follow; output borrows input. */
int mulib_container_ipc_decode (const void *data, size_t length, size_t max_payload,
                                mulib_container_ipc_message *message, size_t *consumed);
/** @brief Initialize uninitialized reader; payload limit must fit 32 bits. */
int mulib_container_ipc_reader_init (mulib_container_ipc_reader *reader, size_t max_payload);
/** @brief Consume only through the first frame. DONE means peek is available;
 * while ready, feed consumes zero. Input is copied, never retained. */
int mulib_container_ipc_feed (mulib_container_ipc_reader *reader,
                              const void *data, size_t length, size_t *consumed);
/** @brief Borrow assembled message until reset/free; returns AGAIN if incomplete. */
int mulib_container_ipc_peek (const mulib_container_ipc_reader *reader,
                              mulib_container_ipc_message *message);
/** @brief Discard a frame/error, retaining bounded allocated storage. */
void mulib_container_ipc_reset (mulib_container_ipc_reader *reader);
/** @brief Release all owned storage and reset. */
void mulib_container_ipc_reader_free (mulib_container_ipc_reader *reader);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINER_IPC) && !defined(MULIB_CONTAINER_IPC_IMPLEMENTED)
#define MULIB_CONTAINER_IPC_IMPLEMENTED
static int
mulib_container_ipc_header_ (const unsigned char *p, size_t max, size_t *total)
{
  uint32_t n, status;
  if (memcmp (p, "MCIP", 4) || p[4] != 1 || (p[5] & ~1U)
      || !(p[6] | p[7])) return MULIB_CT_EFORMAT;
  if (mulib_crc32 (p, 28) != mulib_ct_load32 (p + 28)) return MULIB_CT_ECHECKSUM;
  n = mulib_ct_load32 (p + 16); status = mulib_ct_load32 (p + 20);
  if ((status && status < UINT32_MAX - 13) || (!(p[5] & 1) && status))
    return MULIB_CT_EFORMAT;
  if (n > max || mulib_ct_add_size (32, n, total)) return MULIB_CT_ELIMIT;
  return 0;
}
int
mulib_container_ipc_encode_into (const mulib_container_ipc_message *m,
    void *output, size_t capacity, size_t max, size_t *written)
{
  unsigned char h[32] = { 0 }, *p = (unsigned char *)output;
  size_t total;
  if (written) *written = 0;
  if (!m || !p || !written || !m->opcode || m->flags > 1 || (!m->data && m->length)
      || m->status > 0 || m->status < MULIB_CT_ECHECKSUM || (!m->flags && m->status))
    return MULIB_CT_EINVAL;
  if (m->length > max || m->length > UINT32_MAX
      || mulib_ct_add_size (32, m->length, &total) || total > capacity) return MULIB_CT_ELIMIT;
  memcpy (h, "MCIP", 4); h[4] = 1; h[5] = (unsigned char)m->flags;
  h[6] = (unsigned char)m->opcode; h[7] = (unsigned char)(m->opcode >> 8);
  mulib_ct_store64 (h + 8, m->request_id); mulib_ct_store32 (h + 16, (uint32_t)m->length);
  mulib_ct_store32 (h + 20, (uint32_t)m->status);
  mulib_ct_store32 (h + 24, mulib_crc32 (m->data, m->length));
  mulib_ct_store32 (h + 28, mulib_crc32 (h, 28));
  memcpy (p, h, 32); if (m->length) memcpy (p + 32, m->data, m->length);
  *written = total;
  return 0;
}
int
mulib_container_ipc_encode (const mulib_container_ipc_message *m,
                            mulib_ct_blob *out, size_t max)
{
  mulib_ct_blob next = { NULL, 0, 0 };
  unsigned char h[32] = { 0 };
  size_t total;
  int rc;
  if (!m || !out || !m->opcode || m->flags > 1 || (!m->data && m->length)
      || m->status > 0 || m->status < MULIB_CT_ECHECKSUM
      || (!m->flags && m->status)) return MULIB_CT_EINVAL;
  if (m->length > max || m->length > UINT32_MAX
      || mulib_ct_add_size (32, m->length, &total)) return MULIB_CT_ELIMIT;
  memcpy (h, "MCIP", 4); h[4] = 1; h[5] = (unsigned char)m->flags;
  h[6] = (unsigned char)m->opcode; h[7] = (unsigned char)(m->opcode >> 8);
  mulib_ct_store64 (h + 8, m->request_id); mulib_ct_store32 (h + 16, (uint32_t)m->length);
  mulib_ct_store32 (h + 20, (uint32_t)m->status);
  mulib_ct_store32 (h + 24, mulib_crc32 (m->data, m->length));
  mulib_ct_store32 (h + 28, mulib_crc32 (h, 28));
  rc = mulib_ct_blob_reserve (&next, total, total);
  if (!rc) rc = mulib_ct_blob_append (&next, h, 32, total);
  if (!rc) rc = mulib_ct_blob_append (&next, m->data, m->length, total);
  if (rc) { mulib_ct_blob_free (&next); return rc; }
  mulib_ct_blob_free (out); *out = next;
  return 0;
}
int
mulib_container_ipc_decode (const void *data, size_t n, size_t max,
                            mulib_container_ipc_message *out, size_t *consumed)
{
  const unsigned char *p = (const unsigned char *)data;
  mulib_container_ipc_message m;
  size_t total;
  uint32_t status;
  int rc;
  if (consumed) *consumed = 0;
  if ((!p && n) || !out || !consumed) return MULIB_CT_EINVAL;
  if (n < 32) return MULIB_CT_AGAIN;
  rc = mulib_container_ipc_header_ (p, max, &total);
  if (rc) return rc;
  if (n < total) return MULIB_CT_AGAIN;
  if (mulib_crc32 (p + 32, total - 32) != mulib_ct_load32 (p + 24)) return MULIB_CT_ECHECKSUM;
  memset (&m, 0, sizeof (m)); m.flags = p[5];
  m.opcode = (uint16_t)((unsigned)p[6] | (unsigned)p[7] << 8);
  m.request_id = mulib_ct_load64 (p + 8); status = mulib_ct_load32 (p + 20);
  m.status = status ? -1 - (int)(UINT32_MAX - status) : 0;
  m.data = p + 32; m.length = total - 32; *out = m; *consumed = total;
  return 0;
}
int
mulib_container_ipc_reader_init (mulib_container_ipc_reader *r, size_t max)
{
  if (!r || max > UINT32_MAX || max > SIZE_MAX - 32) return MULIB_CT_EINVAL;
  memset (r, 0, sizeof (*r)); r->max_payload = max; r->expected = 32;
  return 0;
}
void mulib_container_ipc_reset (mulib_container_ipc_reader *r)
{
  if (r) { r->bytes.length = 0; r->expected = 32; r->ready = r->error = 0; }
}
void mulib_container_ipc_reader_free (mulib_container_ipc_reader *r)
{
  if (r) { mulib_ct_blob_free (&r->bytes); memset (r, 0, sizeof (*r)); }
}
int
mulib_container_ipc_feed (mulib_container_ipc_reader *r, const void *data,
                          size_t n, size_t *consumed)
{
  const unsigned char *p = (const unsigned char *)data;
  int rc;
  if (consumed) *consumed = 0;
  if (!r || !consumed || (!p && n) || r->expected < 32) return MULIB_CT_EINVAL;
  if (r->error) return r->error;
  if (r->ready) return MULIB_CT_DONE;
  while (n)
    {
      size_t take = r->expected - r->bytes.length;
      if (take > n) take = n;
      rc = mulib_ct_blob_append (&r->bytes, p, take, r->max_payload + 32);
      if (rc) return r->error = rc;
      p += take; n -= take; *consumed += take;
      if (r->bytes.length < r->expected) break;
      if (r->expected == 32)
        {
          rc = mulib_container_ipc_header_ (r->bytes.data, r->max_payload, &r->expected);
          if (rc) return r->error = rc;
        }
      if (r->bytes.length == r->expected)
        {
          mulib_container_ipc_message message;
          size_t used;
          rc = mulib_container_ipc_decode (r->bytes.data, r->bytes.length,
                                           r->max_payload, &message, &used);
          if (rc) return r->error = rc;
          r->ready = 1; return MULIB_CT_DONE;
        }
    }
  return MULIB_CT_AGAIN;
}
int
mulib_container_ipc_peek (const mulib_container_ipc_reader *r, mulib_container_ipc_message *m)
{
  size_t used;
  if (!r || !m) return MULIB_CT_EINVAL;
  if (r->error) return r->error;
  if (!r->ready) return MULIB_CT_AGAIN;
  return mulib_container_ipc_decode (r->bytes.data, r->bytes.length, r->max_payload, m, &used);
}
#endif
