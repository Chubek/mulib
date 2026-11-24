/** @file mulib-containerd-ctrlbridge.h
 * @brief Incremental authorized control connection with backpressure and replay protection.
 * Define MULIB_IMPL_CONTAINERD_CTRLBRIDGE and dependency switches explicitly. */
#ifndef MULIB_CONTAINERD_CTRLBRIDGE_H
#define MULIB_CONTAINERD_CTRLBRIDGE_H
#include "mulib-containerd.h"
#include "mulib-containerd-accessstub.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief One authenticated connection. Exact last-request retries replay the
 * cached reply; reused IDs with different bytes and older IDs are rejected. */
typedef struct mulib_containerd_ctrlbridge
{
  mulib_containerd *daemon;
  mulib_containerd_accessstub *policy;
  mulib_containerd_principal principal;
  mulib_container_ipc_reader reader;
  unsigned char request[128], reply[80];
  size_t request_length, reply_length, sent;
  uint64_t last_request_id;
  int have_reply, pending, busy, error;
} mulib_containerd_ctrlbridge;
/** @brief Borrow daemon/policy and copy trusted principal. */
int mulib_containerd_ctrlbridge_init (mulib_containerd_ctrlbridge *bridge,
    mulib_containerd *daemon, mulib_containerd_accessstub *policy,
    const mulib_containerd_principal *principal);
/** @brief Feed through one request. DONE means reply available. While a reply
 * is pending, input consumes zero and returns AGAIN. now replaces wire time.
 * Framing/replay errors are sticky until connection destruction. */
int mulib_containerd_ctrlbridge_feed (mulib_containerd_ctrlbridge *bridge,
    const void *bytes, size_t length, size_t *consumed, uint64_t now);
/** @brief Copy a reply prefix. DONE means the pending reply is fully drained. */
int mulib_containerd_ctrlbridge_drain (mulib_containerd_ctrlbridge *bridge,
    void *output, size_t capacity, size_t *written);
/** @brief Release accumulator; borrowed daemon/policy remain alive. */
void mulib_containerd_ctrlbridge_free (mulib_containerd_ctrlbridge *bridge);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINERD_CTRLBRIDGE) && !defined(MULIB_CONTAINERD_CTRLBRIDGE_IMPLEMENTED)
#define MULIB_CONTAINERD_CTRLBRIDGE_IMPLEMENTED
int
mulib_containerd_ctrlbridge_init (mulib_containerd_ctrlbridge *b, mulib_containerd *d,
    mulib_containerd_accessstub *policy, const mulib_containerd_principal *principal)
{
  mulib_containerd_ctrlbridge next;
  int rc;
  if (!b || !d || !d->initialized || !policy || !principal) return MULIB_CT_EINVAL;
  memset (&next, 0, sizeof (next)); next.daemon = d; next.policy = policy; next.principal = *principal;
  rc = mulib_container_ipc_reader_init (&next.reader, 96);
  if (rc) return rc;
  *b = next;
  return 0;
}
int
mulib_containerd_ctrlbridge_feed (mulib_containerd_ctrlbridge *b,
    const void *data, size_t n, size_t *used, uint64_t now)
{
  mulib_container_ipc_message m, response;
  mulib_conainerd_command command;
  mulib_conainerd_status status;
  unsigned char payload[48] = { 0 };
  int rc, result;
  if (used) *used = 0;
  if (!b || !used || !b->daemon) return MULIB_CT_EINVAL;
  if (b->busy) return MULIB_CT_EBUSY;
  if (b->error) return b->error;
  if (b->pending) return MULIB_CT_AGAIN;
  rc = mulib_container_ipc_feed (&b->reader, data, n, used);
  if (rc != MULIB_CT_DONE) return rc < 0 ? (b->error = rc) : rc;
  rc = mulib_container_ipc_peek (&b->reader, &m);
  if (!rc) rc = mulib_conainerd_ipc_parse (&m, &command);
  if (rc) return b->error = rc;
  if (!command.request_id) return b->error = MULIB_CT_EFORMAT;
  if (b->have_reply && command.request_id <= b->last_request_id)
    {
      if (command.request_id != b->last_request_id || b->request_length != b->reader.bytes.length
          || memcmp (b->request, b->reader.bytes.data, b->request_length))
        return b->error = MULIB_CT_ESTALE;
      b->sent = 0; b->pending = 1; mulib_container_ipc_reset (&b->reader);
      return MULIB_CT_DONE;
    }
  /* All response storage exists before invoking a side-effecting command. */
  command.now = now; b->busy = 1;
  result = mulib_containerd_accessstub_check (b->policy, &b->principal, &command, NULL);
  if (!result) result = mulib_containerd_dispatch (b->daemon, &command, &status);
  b->busy = 0;
  if (result > 0 || result < MULIB_CT_ECHECKSUM) result = MULIB_CT_EIO;
  memset (&response, 0, sizeof (response)); response.opcode = m.opcode;
  response.flags = MULIB_CONTAINER_IPC_RESPONSE; response.request_id = m.request_id;
  response.status = result;
  if (!result)
    {
      mulib_ct_store32 (payload, (uint32_t)status.state);
      mulib_ct_store32 (payload + 4, (uint32_t)status.exit_code);
      mulib_ct_store64 (payload + 8, status.generation); mulib_ct_store64 (payload + 16, status.starts);
      mulib_ct_store64 (payload + 24, status.started_at); mulib_ct_store64 (payload + 32, status.exited_at);
      mulib_ct_store64 (payload + 40, status.polls); response.data = payload; response.length = 48;
    }
  rc = mulib_container_ipc_encode_into (&response, b->reply, sizeof (b->reply), 48, &b->reply_length);
  if (rc) return b->error = rc;
  b->request_length = b->reader.bytes.length;
  memcpy (b->request, b->reader.bytes.data, b->request_length);
  b->last_request_id = m.request_id; b->have_reply = b->pending = 1; b->sent = 0;
  mulib_container_ipc_reset (&b->reader);
  return MULIB_CT_DONE;
}
int
mulib_containerd_ctrlbridge_drain (mulib_containerd_ctrlbridge *b, void *out, size_t cap, size_t *written)
{
  size_t take;
  if (written) *written = 0;
  if (!b || !written || (!out && cap)) return MULIB_CT_EINVAL;
  if (b->busy) return MULIB_CT_EBUSY;
  if (b->error) return b->error;
  if (!b->pending) return MULIB_CT_AGAIN;
  take = b->reply_length - b->sent; if (take > cap) take = cap;
  if (take) memcpy (out, b->reply + b->sent, take);
  b->sent += take; *written = take;
  if (b->sent == b->reply_length) { b->pending = 0; return MULIB_CT_DONE; }
  return MULIB_CT_AGAIN;
}
void mulib_containerd_ctrlbridge_free (mulib_containerd_ctrlbridge *b)
{
  if (!b || b->busy) return;
  mulib_container_ipc_reader_free (&b->reader); memset (b, 0, sizeof (*b));
}
#endif
