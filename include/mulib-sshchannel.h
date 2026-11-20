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

/* Optional managed dispatch. Zero-initialize a fixed caller-owned slot array
 * for a connection. The application selects local IDs and authorizes opens
 * and requests. Slot indices are not wire IDs. */
typedef enum {
  MULIB_SSH_CHANNEL_FREE, MULIB_SSH_CHANNEL_OPEN_SENT, MULIB_SSH_CHANNEL_OPEN_RECEIVED,
  MULIB_SSH_CHANNEL_ACTIVE
} mulib_sshchannel_state_t;

enum {
  MULIB_SSH_OPEN_ADMINISTRATIVELY_PROHIBITED = 1, MULIB_SSH_OPEN_CONNECT_FAILED,
  MULIB_SSH_OPEN_UNKNOWN_CHANNEL_TYPE, MULIB_SSH_OPEN_RESOURCE_SHORTAGE
};

typedef struct {
  mulib_sshchannel_state_t state;
  mulib_sshchannel_t channel;
  uint32_t uncredited, replies_to_send, replies_expected;
} mulib_sshchannel_slot_t;

typedef struct {
  size_t slot;
  int released;
  mulib_ssh_channel_endpoint_t endpoint;
  mulib_ssh_channel_event_t message;
} mulib_sshchannel_dispatch_event_t;

typedef struct {
  size_t slot;
  unsigned char type; /* One of the SSH_MSG_CHANNEL_* numbers. */
  mulib_ssh_channel_endpoint_t endpoint; /* OPEN, OPEN_CONFIRMATION */
  uint32_t value; /* OPEN_FAILURE reason, EXTENDED_DATA type, WINDOW_ADJUST credit */
  int want_reply; /* REQUEST */
  mulib_ssh_slice_t name, data; /* REQUEST name/body, DATA, OPEN_FAILURE description */
} mulib_sshchannel_command_t;

/* Dispatch one payload (1/0). Event slices borrow input. An unhandled OPEN
 * (unsupported type or table full) reports slot=SIZE_MAX; send OPEN_FAILURE
 * to event.endpoint.id. Its value is the suggested MULIB_SSH_OPEN_* reason.
 * An OPEN_FAILURE or the second CLOSE releases the corresponding slot.
 * Answer a CLOSE with released=0 with local CLOSE. Traffic preceding the
 * peer's CLOSE may still arrive after local CLOSE, but cannot be answered. */
int mulib_sshchannel_dispatch_receive (mulib_sshchannel_slot_t *, size_t,
                                        const void *, size_t, mulib_sshchannel_dispatch_event_t *);
/* Encode a payload and update just command.slot (1/0). Any failure leaves
 * slot state unchanged. WINDOW_ADJUST can return only credit consumed by data.
 * Replies match want_reply requests in wire order per direction and channel;
 * the caller orders responses to its incoming requests. Queue this output
 * before sending another message, or use the atomic connection driver. */
int mulib_sshchannel_dispatch_send (mulib_sshchannel_slot_t *, size_t,
                                     const mulib_sshchannel_command_t *, void *, size_t, size_t *used);
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

static int
mulib_sshchannel_id_used_ (const mulib_sshchannel_slot_t *slots, size_t count,
                            uint32_t id, int peer)
{
  for (size_t i = 0; i < count; ++i) {
    const mulib_sshchannel_slot_t *s = &slots[i];
    if (s->state != MULIB_SSH_CHANNEL_FREE
        && s->state != (peer ? MULIB_SSH_CHANNEL_OPEN_SENT : MULIB_SSH_CHANNEL_OPEN_RECEIVED)
        && id == (peer ? s->channel.peer.id : s->channel.local.id)) return 1;
  }
  return 0;
}

int
mulib_sshchannel_dispatch_receive (mulib_sshchannel_slot_t *slots, size_t count,
                                    const void *data, size_t n, mulib_sshchannel_dispatch_event_t *out)
{
  mulib_ssh_reader_t r = {(const unsigned char *)data, n};
  mulib_sshchannel_dispatch_event_t event;
  mulib_sshchannel_slot_t next;
  uint32_t id;
  memset (&event, 0, sizeof event);
  if ((!slots && count) || !out || n > MULIB_SSH_PAYLOAD_MAX
      || !mulib_ssh_read_u8 (&r, &event.message.type)) return 0;
  if (event.message.type == MULIB_SSH_CHANNEL_OPEN) {
    mulib_ssh_channel_endpoint_t *ep = &event.endpoint;
    if (!mulib_ssh_read_string (&r, &event.message.name)
        || !mulib_sshchannel_request_name_ (event.message.name)
        || !mulib_ssh_read_u32 (&r, &ep->id) || !mulib_ssh_read_u32 (&r, &ep->window)
        || !mulib_ssh_read_u32 (&r, &ep->max_packet) || !ep->max_packet
        || mulib_sshchannel_id_used_ (slots, count, ep->id, 1)) return 0;
    event.slot = SIZE_MAX;
    if (!mulib_ssh_slice_is (event.message.name, "session")) {
      event.message.value = MULIB_SSH_OPEN_UNKNOWN_CHANNEL_TYPE;
    } else {
      if (r.left) return 0;
      event.message.value = MULIB_SSH_OPEN_RESOURCE_SHORTAGE;
      for (size_t i = 0; i < count; ++i)
        if (slots[i].state == MULIB_SSH_CHANNEL_FREE) {
          memset (&next, 0, sizeof next);
          next.state = MULIB_SSH_CHANNEL_OPEN_RECEIVED;
          next.channel.peer = *ep;
          slots[i] = next;
          event.slot = i;
          event.message.value = 0;
          break;
        }
    }
    *out = event;
    return 1;
  }
  if (!mulib_ssh_read_u32 (&r, &id)) return 0;
  for (event.slot = 0; event.slot < count; ++event.slot)
    if ((slots[event.slot].state == MULIB_SSH_CHANNEL_OPEN_SENT
         || slots[event.slot].state == MULIB_SSH_CHANNEL_ACTIVE)
        && slots[event.slot].channel.local.id == id) break;
  if (event.slot == count) return 0;
  next = slots[event.slot];
  if (next.state == MULIB_SSH_CHANNEL_OPEN_SENT) {
    if (event.message.type == MULIB_SSH_CHANNEL_OPEN_CONFIRMATION) {
      if (!mulib_sshchannel_confirm_parse (data, n, id, &event.endpoint)
          || mulib_sshchannel_id_used_ (slots, count, event.endpoint.id, 1)
          || !mulib_sshchannel_init (&next.channel, next.channel.local, event.endpoint)) return 0;
      next.state = MULIB_SSH_CHANNEL_ACTIVE;
    } else if (event.message.type == MULIB_SSH_CHANNEL_OPEN_FAILURE) {
      if (!mulib_sshchannel_failure_parse (data, n, id, &event.message.value, &event.message.data))
        return 0;
      event.released = 1;
    } else return 0;
  } else {
    if (!mulib_sshchannel_receive (&next.channel, data, n, &event.message)) return 0;
    switch (event.message.type) {
      case MULIB_SSH_CHANNEL_DATA:
      case MULIB_SSH_CHANNEL_EXTENDED_DATA:
        if (event.message.data.length > UINT32_MAX - next.uncredited) return 0;
        next.uncredited += (uint32_t)event.message.data.length;
        break;
      case MULIB_SSH_CHANNEL_REQUEST:
        if (event.message.want_reply && !next.channel.sent_close) {
          if (next.replies_to_send == UINT32_MAX) return 0;
          ++next.replies_to_send;
        }
        break;
      case MULIB_SSH_CHANNEL_SUCCESS:
      case MULIB_SSH_CHANNEL_FAILURE:
        if (!next.replies_expected) return 0;
        --next.replies_expected;
        break;
      case MULIB_SSH_CHANNEL_CLOSE:
        event.released = next.channel.sent_close != 0;
        break;
    }
  }
  if (event.released) memset (&next, 0, sizeof next);
  slots[event.slot] = next;
  *out = event;
  return 1;
}

int
mulib_sshchannel_dispatch_send (mulib_sshchannel_slot_t *slots, size_t count,
                                 const mulib_sshchannel_command_t *cmd,
                                 void *out, size_t cap, size_t *used)
{
  mulib_sshchannel_slot_t next;
  if (used) *used = 0;
  if (!slots || !cmd || cmd->slot >= count || !used) return 0;
  next = slots[cmd->slot];
  if (cmd->type == MULIB_SSH_CHANNEL_OPEN) {
    if (next.state != MULIB_SSH_CHANNEL_FREE
        || mulib_sshchannel_id_used_ (slots, count, cmd->endpoint.id, 0)
        || !mulib_sshchannel_open_write (cmd->endpoint, out, cap, used)) return 0;
    memset (&next, 0, sizeof next);
    next.channel.local = cmd->endpoint;
    next.state = MULIB_SSH_CHANNEL_OPEN_SENT;
  } else if (next.state == MULIB_SSH_CHANNEL_OPEN_RECEIVED) {
    uint32_t peer = next.channel.peer.id;
    if (cmd->type == MULIB_SSH_CHANNEL_OPEN_CONFIRMATION) {
      if (mulib_sshchannel_id_used_ (slots, count, cmd->endpoint.id, 0)
          || !mulib_sshchannel_confirm_write (peer, cmd->endpoint, out, cap, used)) return 0;
      next.channel.local = cmd->endpoint;
      next.state = MULIB_SSH_CHANNEL_ACTIVE;
    } else if (cmd->type == MULIB_SSH_CHANNEL_OPEN_FAILURE) {
      if (!mulib_sshchannel_failure_write (peer, cmd->value, cmd->data, out, cap, used)) return 0;
      memset (&next, 0, sizeof next);
    } else return 0;
  } else if (next.state == MULIB_SSH_CHANNEL_ACTIVE) {
    switch (cmd->type) {
      case MULIB_SSH_CHANNEL_DATA:
      case MULIB_SSH_CHANNEL_EXTENDED_DATA:
        if ((cmd->type == MULIB_SSH_CHANNEL_EXTENDED_DATA && !cmd->value)
            || !mulib_sshchannel_data (&next.channel,
                                       cmd->type == MULIB_SSH_CHANNEL_DATA ? 0 : cmd->value,
                                       cmd->data, out, cap, used)) return 0;
        break;
      case MULIB_SSH_CHANNEL_WINDOW_ADJUST:
        if (cmd->value > next.uncredited
            || !mulib_sshchannel_window (&next.channel, cmd->value, out, cap, used)) return 0;
        next.uncredited -= cmd->value;
        break;
      case MULIB_SSH_CHANNEL_REQUEST:
        if ((cmd->want_reply && next.replies_expected == UINT32_MAX)
            || !mulib_sshchannel_request (&next.channel, cmd->name, cmd->want_reply,
                                          cmd->data, out, cap, used)) return 0;
        next.replies_expected += (unsigned)cmd->want_reply;
        break;
      case MULIB_SSH_CHANNEL_SUCCESS:
      case MULIB_SSH_CHANNEL_FAILURE:
        if (!next.replies_to_send
            || !mulib_sshchannel_control (&next.channel, cmd->type, out, cap, used)) return 0;
        --next.replies_to_send;
        break;
      case MULIB_SSH_CHANNEL_EOF:
      case MULIB_SSH_CHANNEL_CLOSE:
        if (!mulib_sshchannel_control (&next.channel, cmd->type, out, cap, used)) return 0;
        if (next.channel.sent_close && next.channel.received_close) memset (&next, 0, sizeof next);
        break;
      default:
        return 0;
    }
  } else return 0;
  slots[cmd->slot] = next;
  return 1;
}
#endif
