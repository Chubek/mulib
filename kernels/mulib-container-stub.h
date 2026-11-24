/** @file mulib-container-stub.h
 * @brief Nonblocking lifecycle client with partial I/O, correlation and bounded replies.
 * Define MULIB_IMPL_CONTAINER_STUB and CONAINERD_IPC dependencies. */
#ifndef MULIB_CONTAINER_STUB_H
#define MULIB_CONTAINER_STUB_H
#include "mulib-conainerd-ipc.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief I/O returns OK with transferred prefix, AGAIN with zero progress,
 * or negative CT error. A zero-byte OK is treated as AGAIN. */
typedef struct mulib_container_stub_transport
{
  int (*write) (void *, const void *, size_t, size_t *);
  int (*read) (void *, void *, size_t, size_t *);
  void *user;
} mulib_container_stub_transport;
typedef enum mulib_container_stub_state
{ MULIB_CONTAINER_STUB_IDLE, MULIB_CONTAINER_STUB_SENDING,
  MULIB_CONTAINER_STUB_RECEIVING, MULIB_CONTAINER_STUB_READY,
  MULIB_CONTAINER_STUB_FAILED } mulib_container_stub_state;
typedef struct mulib_container_stub
{
  mulib_container_stub_transport transport;
  mulib_conainerd_command command;
  mulib_container_ipc_reader reader;
  mulib_ct_blob request;
  size_t sent;
  uint64_t next_request_id;
  mulib_container_stub_state state;
  int error, busy;
} mulib_container_stub;
/** @brief Initialize with borrowed transport; first request ID must be nonzero. */
int mulib_container_stub_init (mulib_container_stub *client,
    const mulib_container_stub_transport *transport, uint64_t first_request_id);
/** @brief Copy/encode one command, replacing its request ID with a monotonic client ID. */
int mulib_container_stub_begin (mulib_container_stub *client, const mulib_conainerd_command *command);
/** @brief Make at most io_budget callbacks; AGAIN preserves exact progress. */
int mulib_container_stub_pump (mulib_container_stub *client, size_t io_budget);
/** @brief Consume a correlated reply and return remote status, restoring IDLE. */
int mulib_container_stub_take (mulib_container_stub *client, mulib_conainerd_status *status);
/** @brief Release buffers; caller closes/resynchronizes the transport after cancellation. */
void mulib_container_stub_free (mulib_container_stub *client);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINER_STUB) && !defined(MULIB_CONTAINER_STUB_IMPLEMENTED)
#define MULIB_CONTAINER_STUB_IMPLEMENTED
int mulib_container_stub_init (mulib_container_stub *c,
    const mulib_container_stub_transport *t, uint64_t first)
{
  mulib_container_stub next;
  int rc;
  if (!c || !t || !t->read || !t->write || !first) return MULIB_CT_EINVAL;
  memset (&next, 0, sizeof (next)); next.transport = *t; next.next_request_id = first;
  rc = mulib_container_ipc_reader_init (&next.reader, 48);
  if (rc) return rc;
  *c = next;
  return 0;
}
int mulib_container_stub_begin (mulib_container_stub *c, const mulib_conainerd_command *command)
{
  mulib_conainerd_command next;
  int rc;
  if (!c || !command || !c->transport.read) return MULIB_CT_EINVAL;
  if (c->busy) return MULIB_CT_EBUSY;
  if (c->state != MULIB_CONTAINER_STUB_IDLE) return MULIB_CT_ESTATE;
  if (c->next_request_id == UINT64_MAX) return MULIB_CT_ELIMIT;
  next = *command; next.request_id = c->next_request_id;
  rc = mulib_conainerd_ipc_command (&next, &c->request);
  if (rc) return rc;
  ++c->next_request_id; c->command = next; c->sent = 0;
  c->state = MULIB_CONTAINER_STUB_SENDING; mulib_container_ipc_reset (&c->reader);
  return 0;
}
static int mulib_container_stub_fail_ (mulib_container_stub *c, int rc)
{ c->state = MULIB_CONTAINER_STUB_FAILED; c->error = rc; c->busy = 0; return rc; }
int mulib_container_stub_pump (mulib_container_stub *c, size_t budget)
{
  size_t i;
  if (!c || !c->transport.read) return MULIB_CT_EINVAL;
  if (c->busy) return MULIB_CT_EBUSY;
  if (c->state == MULIB_CONTAINER_STUB_FAILED) return c->error;
  if (c->state == MULIB_CONTAINER_STUB_READY) return MULIB_CT_DONE;
  if (c->state == MULIB_CONTAINER_STUB_IDLE) return MULIB_CT_ESTATE;
  c->busy = 1;
  for (i = 0; i < budget; ++i)
    {
      size_t done = 0, needed;
      int rc;
      if (c->state == MULIB_CONTAINER_STUB_SENDING)
        {
          needed = c->request.length - c->sent;
          rc = c->transport.write (c->transport.user, c->request.data + c->sent, needed, &done);
          if (done > needed || (rc && done)) return mulib_container_stub_fail_ (c, MULIB_CT_EIO);
          if (rc < 0) return mulib_container_stub_fail_ (c, rc);
          if (rc > MULIB_CT_AGAIN) return mulib_container_stub_fail_ (c, MULIB_CT_EIO);
          c->sent += done;
          if (c->sent == c->request.length) c->state = MULIB_CONTAINER_STUB_RECEIVING;
        }
      else
        {
          unsigned char bytes[80];
          size_t consumed;
          needed = c->reader.expected - c->reader.bytes.length;
          if (needed > sizeof (bytes)) needed = sizeof (bytes);
          rc = c->transport.read (c->transport.user, bytes, needed, &done);
          if (done > needed || (rc && done)) return mulib_container_stub_fail_ (c, MULIB_CT_EIO);
          if (rc < 0) return mulib_container_stub_fail_ (c, rc);
          if (rc > MULIB_CT_AGAIN) return mulib_container_stub_fail_ (c, MULIB_CT_EIO);
          if (done)
            {
              rc = mulib_container_ipc_feed (&c->reader, bytes, done, &consumed);
              if (rc < 0) return mulib_container_stub_fail_ (c, rc);
              if (rc == MULIB_CT_DONE)
                { c->state = MULIB_CONTAINER_STUB_READY; c->busy = 0; return MULIB_CT_DONE; }
            }
        }
      if (!done) break;
    }
  c->busy = 0;
  return MULIB_CT_AGAIN;
}
int mulib_container_stub_take (mulib_container_stub *c, mulib_conainerd_status *out)
{
  mulib_container_ipc_message message;
  int rc;
  if (!c || !out) return MULIB_CT_EINVAL;
  if (c->busy) return MULIB_CT_EBUSY;
  if (c->state != MULIB_CONTAINER_STUB_READY) return MULIB_CT_ESTATE;
  rc = mulib_container_ipc_peek (&c->reader, &message);
  if (rc) return mulib_container_stub_fail_ (c, rc);
  rc = mulib_conainerd_ipc_status (&message, &c->command, out);
  if (message.request_id != c->command.request_id || message.opcode != c->command.opcode
      || message.flags != MULIB_CONTAINER_IPC_RESPONSE || (!message.status && rc)
      || (message.status && message.length))
    return mulib_container_stub_fail_ (c, MULIB_CT_EFORMAT);
  mulib_container_ipc_reset (&c->reader); c->request.length = 0;
  c->state = MULIB_CONTAINER_STUB_IDLE;
  return rc;
}
void mulib_container_stub_free (mulib_container_stub *c)
{
  if (!c || c->busy) return;
  mulib_container_ipc_reader_free (&c->reader); mulib_ct_blob_free (&c->request);
  memset (c, 0, sizeof (*c));
}
#endif
