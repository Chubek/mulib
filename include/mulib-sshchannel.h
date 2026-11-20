/** @file mulib-sshchannel.h
 * @brief SSH session-channel payloads and flow control (RFC 4254).
 * Define MULIB_IMPL_SSHCHANNEL and SSHWIRE.
 */
#ifndef MULIB_SSHCHANNEL_H
#define MULIB_SSHCHANNEL_H
#include "mulib-sshpacket.h"
#include "mulib-sshproto.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { MULIB_SSH_CHANNEL_DATA_MAX = MULIB_SSH_PAYLOAD_MAX - 13 };
typedef struct {
  uint32_t id, window, max_packet;
} mulib_ssh_channel_endpoint_t;

typedef struct {
  mulib_ssh_channel_endpoint_t local, peer;
  unsigned sent_eof, received_eof, sent_close, received_close;
} mulib_sshchannel_t;

typedef struct {
  unsigned char type;
  uint32_t value; /* Extended data type or window increment. */
  int want_reply;
  mulib_ssh_slice_t name, data;
} mulib_ssh_channel_event_t;

/* Application owns channel IDs, dispatch and authorization. Use only after
 * successful user authentication. Encoders require disjoint buffers and
 * set used=0 on failure. Payloads and state commit in transport order. */
int mulib_sshchannel_open_write (mulib_ssh_channel_endpoint_t local, void *, size_t, size_t *used);
int mulib_sshchannel_open_parse (const void *, size_t, mulib_ssh_channel_endpoint_t *);
int mulib_sshchannel_confirm_write (uint32_t recipient, mulib_ssh_channel_endpoint_t local,
                                    void *, size_t, size_t *used);
int mulib_sshchannel_confirm_parse (const void *, size_t, uint32_t recipient,
                                    mulib_ssh_channel_endpoint_t *);
int mulib_sshchannel_failure_write (uint32_t recipient, uint32_t reason,
                                    mulib_ssh_slice_t description, void *, size_t, size_t *used);
int mulib_sshchannel_failure_parse (const void *, size_t, uint32_t recipient,
                                    uint32_t *reason, mulib_ssh_slice_t *description);
int mulib_sshchannel_init (mulib_sshchannel_t *, mulib_ssh_channel_endpoint_t local,
                            mulib_ssh_channel_endpoint_t peer);

/* Consume one message addressed to local.id. Returns 1/0; state and output
 * unchanged on failure. Event slices borrow input. Unknown request names
 * have opaque request-specific bodies for the application's policy. */
int mulib_sshchannel_receive (mulib_sshchannel_t *, const void *, size_t, mulib_ssh_channel_event_t *);
/* extended_type=0 selects normal data. Limit writes to the peer window,
 * peer max packet and MULIB_SSH_CHANNEL_DATA_MAX. */
int mulib_sshchannel_data (mulib_sshchannel_t *, uint32_t extended_type,
                            mulib_ssh_slice_t, void *, size_t, size_t *used);
/* Grant receive credit after the application has consumed buffered bytes. */
int mulib_sshchannel_window (mulib_sshchannel_t *, uint32_t increment, void *, size_t, size_t *used);
/* EOF, CLOSE, SUCCESS, or FAILURE. A channel is reusable only once both
 * directions have exchanged CLOSE. The application tracks pending replies. */
int mulib_sshchannel_control (mulib_sshchannel_t *, unsigned char type, void *, size_t, size_t *used);
int mulib_sshchannel_request (const mulib_sshchannel_t *, mulib_ssh_slice_t name, int want_reply,
                               mulib_ssh_slice_t body, void *, size_t, size_t *used);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SSHCHANNEL) && !defined(MULIB_SSHCHANNEL_IMPLEMENTED)
#define MULIB_SSHCHANNEL_IMPLEMENTED
#include <string.h>

static int
mulib_sshchannel_endpoint_write_ (mulib_ssh_writer_t *w, mulib_ssh_channel_endpoint_t ep)
{
  return ep.max_packet && ep.max_packet <= MULIB_SSH_CHANNEL_DATA_MAX
    && mulib_ssh_write_u32 (w, ep.id)
    && mulib_ssh_write_u32 (w, ep.window) && mulib_ssh_write_u32 (w, ep.max_packet);
}

static int
mulib_sshchannel_endpoint_parse_ (mulib_ssh_reader_t *r, mulib_ssh_channel_endpoint_t *out)
{
  mulib_ssh_channel_endpoint_t ep;
  if (!out || !mulib_ssh_read_u32 (r, &ep.id) || !mulib_ssh_read_u32 (r, &ep.window)
      || !mulib_ssh_read_u32 (r, &ep.max_packet) || !ep.max_packet || r->left) return 0;
  *out = ep;
  return 1;
}

static int
mulib_sshchannel_header_ (mulib_ssh_writer_t *w, unsigned char type, uint32_t recipient)
{
  return mulib_ssh_write_u8 (w, type) && mulib_ssh_write_u32 (w, recipient);
}

static int
mulib_sshchannel_addressed_ (mulib_ssh_reader_t *r, unsigned char *type, uint32_t recipient)
{
  uint32_t id;
  return mulib_ssh_read_u8 (r, type) && mulib_ssh_read_u32 (r, &id) && id == recipient;
}

int
mulib_sshchannel_open_write (mulib_ssh_channel_endpoint_t ep, void *out, size_t cap, size_t *used)
{
  mulib_ssh_writer_t w = {(unsigned char *)out, cap};
  if (used) *used = 0;
  if (!used || !mulib_ssh_write_u8 (&w, MULIB_SSH_CHANNEL_OPEN)
      || !mulib_ssh_write_string (&w, "session", 7) || !mulib_sshchannel_endpoint_write_ (&w, ep)) return 0;
  *used = cap - w.left;
  return 1;
}

int
mulib_sshchannel_open_parse (const void *data, size_t n, mulib_ssh_channel_endpoint_t *out)
{
  mulib_ssh_reader_t r = {(const unsigned char *)data, n};
  unsigned char type;
  mulib_ssh_slice_t name;
  return mulib_ssh_read_u8 (&r, &type) && type == MULIB_SSH_CHANNEL_OPEN
    && mulib_ssh_read_string (&r, &name) && mulib_ssh_slice_is (name, "session")
    && mulib_sshchannel_endpoint_parse_ (&r, out);
}

int
mulib_sshchannel_confirm_write (uint32_t recipient, mulib_ssh_channel_endpoint_t ep,
                                void *out, size_t cap, size_t *used)
{
  mulib_ssh_writer_t w = {(unsigned char *)out, cap};
  if (used) *used = 0;
  if (!used || !mulib_sshchannel_header_ (&w, MULIB_SSH_CHANNEL_OPEN_CONFIRMATION, recipient)
      || !mulib_sshchannel_endpoint_write_ (&w, ep)) return 0;
  *used = cap - w.left;
  return 1;
}

int
mulib_sshchannel_confirm_parse (const void *data, size_t n, uint32_t recipient,
                                mulib_ssh_channel_endpoint_t *out)
{
  mulib_ssh_reader_t r = {(const unsigned char *)data, n};
  unsigned char type;
  return mulib_sshchannel_addressed_ (&r, &type, recipient) && type == MULIB_SSH_CHANNEL_OPEN_CONFIRMATION
    && mulib_sshchannel_endpoint_parse_ (&r, out);
}

int
mulib_sshchannel_failure_write (uint32_t recipient, uint32_t reason,
                                mulib_ssh_slice_t description, void *out, size_t cap, size_t *used)
{
  mulib_ssh_writer_t w = {(unsigned char *)out, cap};
  if (used) *used = 0;
  if (!used || description.length > MULIB_SSH_PAYLOAD_MAX - 17
      || !mulib_sshchannel_header_ (&w, MULIB_SSH_CHANNEL_OPEN_FAILURE, recipient)
      || !mulib_ssh_write_u32 (&w, reason)
      || !mulib_ssh_write_string (&w, description.data, description.length)
      || !mulib_ssh_write_string (&w, NULL, 0)) return 0;
  *used = cap - w.left;
  return 1;
}

int
mulib_sshchannel_failure_parse (const void *data, size_t n, uint32_t recipient,
                                uint32_t *reason, mulib_ssh_slice_t *description)
{
  mulib_ssh_reader_t r = {(const unsigned char *)data, n};
  unsigned char type;
  uint32_t code;
  mulib_ssh_slice_t text, language;
  if (!reason || !description || !mulib_sshchannel_addressed_ (&r, &type, recipient)
      || type != MULIB_SSH_CHANNEL_OPEN_FAILURE || !mulib_ssh_read_u32 (&r, &code)
      || !mulib_ssh_read_string (&r, &text) || !mulib_ssh_read_string (&r, &language)
      || r.left) return 0;
  *reason = code;
  *description = text;
  return 1;
}

int
mulib_sshchannel_init (mulib_sshchannel_t *s, mulib_ssh_channel_endpoint_t local,
                       mulib_ssh_channel_endpoint_t peer)
{
  if (!s || !local.max_packet || local.max_packet > MULIB_SSH_CHANNEL_DATA_MAX || !peer.max_packet) return 0;
  memset (s, 0, sizeof *s);
  s->local = local;
  s->peer = peer;
  return 1;
}

static int
mulib_sshchannel_request_name_ (mulib_ssh_slice_t name)
{
  if (!name.data || !name.length || name.length > 64) return 0;
  for (size_t i = 0; i < name.length; ++i)
    if (name.data[i] < 33 || name.data[i] > 126 || name.data[i] == ',') return 0;
  return 1;
}

int
mulib_sshchannel_receive (mulib_sshchannel_t *s, const void *data, size_t n,
                          mulib_ssh_channel_event_t *out)
{
  mulib_ssh_reader_t r = {(const unsigned char *)data, n};
  mulib_ssh_channel_event_t event;
  memset (&event, 0, sizeof event);
  unsigned char flag;
  if (!s || !out || !s->local.max_packet || s->received_close
      || n > MULIB_SSH_PAYLOAD_MAX || !mulib_sshchannel_addressed_ (&r, &event.type, s->local.id)) return 0;
  switch (event.type) {
    case MULIB_SSH_CHANNEL_EXTENDED_DATA:
    case MULIB_SSH_CHANNEL_DATA:
      if (s->received_eof || (event.type == MULIB_SSH_CHANNEL_EXTENDED_DATA
                             && !mulib_ssh_read_u32 (&r, &event.value))
          || !mulib_ssh_read_string (&r, &event.data) || r.left
          || event.data.length > s->local.window || event.data.length > s->local.max_packet) return 0;
      s->local.window -= (uint32_t)event.data.length;
      break;
    case MULIB_SSH_CHANNEL_WINDOW_ADJUST:
      if (!mulib_ssh_read_u32 (&r, &event.value) || r.left || !event.value
          || event.value > UINT32_MAX - s->peer.window) return 0;
      s->peer.window += event.value;
      break;
    case MULIB_SSH_CHANNEL_EOF:
      if (r.left || s->received_eof) return 0;
      s->received_eof = 1;
      break;
    case MULIB_SSH_CHANNEL_CLOSE:
      if (r.left) return 0;
      s->received_close = 1;
      break;
    case MULIB_SSH_CHANNEL_REQUEST:
      if (!mulib_ssh_read_string (&r, &event.name) || !mulib_sshchannel_request_name_ (event.name)
          || !mulib_ssh_read_u8 (&r, &flag) || !mulib_ssh_read_bytes (&r, r.left, &event.data)) return 0;
      event.want_reply = flag != 0;
      break;
    case MULIB_SSH_CHANNEL_SUCCESS:
    case MULIB_SSH_CHANNEL_FAILURE:
      if (r.left) return 0;
      break;
    default:
      return 0;
  }
  *out = event;
  return 1;
}

int
mulib_sshchannel_data (mulib_sshchannel_t *s, uint32_t extended,
                       mulib_ssh_slice_t data, void *out, size_t cap, size_t *used)
{
  mulib_ssh_writer_t w = {(unsigned char *)out, cap};
  if (used) *used = 0;
  if (!s || !s->peer.max_packet || !used || s->sent_eof || s->sent_close || s->received_close
      || data.length > MULIB_SSH_CHANNEL_DATA_MAX || data.length > s->peer.max_packet
      || data.length > s->peer.window
      || !mulib_sshchannel_header_ (&w, extended ? MULIB_SSH_CHANNEL_EXTENDED_DATA : MULIB_SSH_CHANNEL_DATA, s->peer.id)
      || (extended && !mulib_ssh_write_u32 (&w, extended))
      || !mulib_ssh_write_string (&w, data.data, data.length)) return 0;
  s->peer.window -= (uint32_t)data.length;
  *used = cap - w.left;
  return 1;
}

int
mulib_sshchannel_window (mulib_sshchannel_t *s, uint32_t increment, void *out, size_t cap, size_t *used)
{
  mulib_ssh_writer_t w = {(unsigned char *)out, cap};
  if (used) *used = 0;
  if (!s || !s->peer.max_packet || !used || s->sent_close || s->received_close || !increment
      || increment > UINT32_MAX - s->local.window
      || !mulib_sshchannel_header_ (&w, MULIB_SSH_CHANNEL_WINDOW_ADJUST, s->peer.id)
      || !mulib_ssh_write_u32 (&w, increment)) return 0;
  s->local.window += increment;
  *used = cap - w.left;
  return 1;
}

int
mulib_sshchannel_control (mulib_sshchannel_t *s, unsigned char type, void *out, size_t cap, size_t *used)
{
  mulib_ssh_writer_t w = {(unsigned char *)out, cap};
  if (used) *used = 0;
  if (!s || !s->peer.max_packet || !used || s->sent_close) return 0;
  if (type != MULIB_SSH_CHANNEL_CLOSE) {
    if (s->received_close
        || (type == MULIB_SSH_CHANNEL_EOF ? s->sent_eof
             : type != MULIB_SSH_CHANNEL_SUCCESS && type != MULIB_SSH_CHANNEL_FAILURE)) return 0;
  }
  if (!mulib_sshchannel_header_ (&w, type, s->peer.id)) return 0;
  if (type == MULIB_SSH_CHANNEL_EOF) s->sent_eof = 1;
  if (type == MULIB_SSH_CHANNEL_CLOSE) s->sent_close = 1;
  *used = cap - w.left;
  return 1;
}

int
mulib_sshchannel_request (const mulib_sshchannel_t *s, mulib_ssh_slice_t name, int want_reply,
                          mulib_ssh_slice_t body, void *out, size_t cap, size_t *used)
{
  mulib_ssh_writer_t w = {(unsigned char *)out, cap};
  if (used) *used = 0;
  if (!s || !s->peer.max_packet || !used || s->sent_close || s->received_close
      || !mulib_sshchannel_request_name_ (name) || body.length > MULIB_SSH_PAYLOAD_MAX - 10 - name.length
      || (want_reply != 0 && want_reply != 1)
      || !mulib_sshchannel_header_ (&w, MULIB_SSH_CHANNEL_REQUEST, s->peer.id)
      || !mulib_ssh_write_string (&w, name.data, name.length)
      || !mulib_ssh_write_u8 (&w, (unsigned char)want_reply)
      || !mulib_ssh_write_bytes (&w, body.data, body.length)) return 0;
  *used = cap - w.left;
  return 1;
}
#endif
