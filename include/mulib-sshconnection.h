/** @file mulib-sshconnection.h
 * @brief Socket-independent SSH connection driver for pinned client and server.
 * Define MULIB_IMPL_SSHCONNECTION, SSHTRANSPORT, SSHAUTH and SSHCHANNEL (and
 * their dependencies). Legacy client and server implementation switches are
 * not needed.
 */
#ifndef MULIB_SSHCONNECTION_H
#define MULIB_SSHCONNECTION_H
#include "mulib-sshserver.h"
#include "mulib-sshchannel.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  MULIB_SSH_CONNECTION_NONE, MULIB_SSH_CONNECTION_IDENT_BANNER,
  MULIB_SSH_CONNECTION_AUTH_SERVICE, MULIB_SSH_CONNECTION_AUTH_REPLY,
  MULIB_SSH_CONNECTION_AUTH_PK_OK, MULIB_SSH_CONNECTION_CHANNEL
} mulib_sshconnection_event_type_t;

typedef struct {
  mulib_sshconnection_event_type_t type;
  mulib_ssh_slice_t text; /* IDENT_BANNER line, or raw Ed25519 key for AUTH_PK_OK */
  mulib_ssh_auth_reply_t auth; /* Client replies/banner; server success only. */
  mulib_sshchannel_dispatch_event_t channel;
} mulib_sshconnection_event_t;

typedef enum {
  MULIB_SSH_CONNECTION_AUTH_NONE = 1, MULIB_SSH_CONNECTION_AUTH_PASSWORD,
  MULIB_SSH_CONNECTION_AUTH_QUERY, MULIB_SSH_CONNECTION_AUTH_SIGNED
} mulib_sshconnection_auth_kind_t;

typedef struct {
  /* Private. Allocate in caller-owned storage, serialize access, and do not
   * copy or modify a live connection or its slot array. */
  mulib_sshtransport_t transport;
  mulib_ssh_role_t role;
  unsigned phase, auth_exhausted;
  mulib_sshconnection_auth_kind_t pending_auth;
  mulib_sshserver_policy_t policy;
  mulib_sshchannel_slot_t *slots;
  size_t slot_count, attempts, banner_bytes;
  unsigned char host[32], query_key[32], user[MULIB_SSH_USER_MAX];
  size_t user_length, version_length, rx_start, rx_end, tx_start, tx_end, plain_used;
  unsigned char version[MULIB_SSH_IDENT_MAX];
  unsigned char rx[MULIB_SSH_FRAME_MAX];
  unsigned char tx[MULIB_SSH_FRAME_MAX + MULIB_SSH_KEX_OUTPUT_MIN];
  unsigned char plain[MULIB_SSH_BODY_MAX], encode[MULIB_SSH_PAYLOAD_MAX];
} mulib_sshconnection_t;

/* Start on uninitialized or cleared storage (1/0). Queues local identification.
 * The exact version includes comments, excludes CRLF, and must be SSH-2.0.
 * Host is the client's trusted pin or the server's private seed. A server
 * policy with a positive attempt limit is copied; pass NULL for a client.
 * The caller-owned slot array is reset and must live until connection clear.
 * A zero-length array is allowed. All input and output buffers are disjoint
 * from connection/slot storage except explicitly borrowed event slices. */
int mulib_sshconnection_start (mulib_sshconnection_t *, mulib_ssh_role_t,
                               mulib_ssh_slice_t version, const unsigned char host[32],
                               const mulib_sshserver_policy_t *, mulib_sshchannel_slot_t *, size_t);
/* View pending wire bytes, then consume exactly the bytes the socket accepted.
 * Until the queue is drained, feed can buffer input but will not process it,
 * and application sends make no progress. */
mulib_ssh_slice_t mulib_sshconnection_output (const mulib_sshconnection_t *);
int mulib_sshconnection_consume_output (mulib_sshconnection_t *, size_t);
/* Copy input (*used is the number of bytes accepted) and process at most one
 * line or packet. 1: progress, possibly no event; 0: incomplete or output
 * pending; -1: fatal/invalid, close (*used=0). Call with NULL,0 to drain
 * buffered input. Event is committed only on 1; slices live until the next
 * feed or clear. Client identification banners are capped at 255 bytes per
 * line and 4096 total wire bytes. Treat both kinds of banner as untrusted
 * text when displaying them. A channel OPEN with slot=SIZE_MAX was
 * already refused by the driver. On a channel CLOSE with released=0, the
 * application must send its own CLOSE. */
int mulib_sshconnection_feed (mulib_sshconnection_t *, const void *, size_t,
                              size_t *used, mulib_sshconnection_event_t *);
/* True only after service acceptance, while the client can send a request.
 * Each query, failure, or success resolves the outstanding request; a
 * banner does not. An empty failure method list prevents further requests.
 * No partial-success or multi-factor authentication. */
int mulib_sshconnection_can_auth (const mulib_sshconnection_t *);
int mulib_sshconnection_authenticated (const mulib_sshconnection_t *);
/* Binary-safe authenticated identity, borrowed until clear. Empty until auth. */
mulib_ssh_slice_t mulib_sshconnection_user (const mulib_sshconnection_t *);
/* Application sends queue one encrypted frame. 1 queued, 0 blocked or bad
 * input/state, -1 fatal (connection cleared). Credential is empty for NONE,
 * the password, raw query public key, or private seed, respectively. The
 * driver does not retain plaintext credentials. */
int mulib_sshconnection_auth (mulib_sshconnection_t *, mulib_sshconnection_auth_kind_t,
                              mulib_ssh_slice_t user, mulib_ssh_slice_t credential);
int mulib_sshconnection_banner (mulib_sshconnection_t *, mulib_ssh_slice_t message,
                                mulib_ssh_slice_t language);
int mulib_sshconnection_channel (mulib_sshconnection_t *, const mulib_sshchannel_command_t *);
/* The driver responds to peer-initiated rekey. It does not initiate one
 * itself. If the transport's AEAD budget is low, arrange peer rekey or close. */
int mulib_sshconnection_rekey_due (const mulib_sshconnection_t *);
/* Clear a started, failed, or zero-initialized connection. */
void mulib_sshconnection_clear (mulib_sshconnection_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SSHCONNECTION) && !defined(MULIB_SSHCONNECTION_IMPLEMENTED)
#define MULIB_SSHCONNECTION_IMPLEMENTED
#include <string.h>

enum {
  MULIB_SSH_CONNECTION_IDENT_ = 1, MULIB_SSH_CONNECTION_KEX_,
  MULIB_SSH_CONNECTION_SERVICE_, MULIB_SSH_CONNECTION_USERAUTH_, MULIB_SSH_CONNECTION_ACTIVE_,
  MULIB_SSH_CONNECTION_IDENT_LINE_MAX_ = MULIB_SSH_IDENT_MAX + 2,
  MULIB_SSH_CONNECTION_BANNERS_MAX_ = 4096,
  MULIB_SSH_CONNECTION_GLOBAL_REQUEST_ = 80, MULIB_SSH_CONNECTION_REQUEST_FAILURE_ = 82
};

static int
mulib_sshconnection_version_ (mulib_ssh_slice_t v)
{
  if (!v.data || v.length < 9 || v.length > MULIB_SSH_IDENT_MAX
      || memcmp (v.data, "SSH-2.0-", 8) || v.data[8] == ' ') return 0;
  for (size_t i = 8; i < v.length; ++i)
    if (v.data[i] < 32 || v.data[i] > 126) return 0;
  return 1;
}

void
mulib_sshconnection_clear (mulib_sshconnection_t *s)
{
  if (!s) return;
  if (s->slot_count) memset (s->slots, 0, s->slot_count * sizeof *s->slots);
  mulib_sshtransport_clear (&s->transport);
  mulib_secret_wipe (s, sizeof *s);
}

static int
mulib_sshconnection_fail_ (mulib_sshconnection_t *s)
{
  mulib_sshconnection_clear (s);
  return -1;
}

int
mulib_sshconnection_start (mulib_sshconnection_t *s, mulib_ssh_role_t role,
                           mulib_ssh_slice_t version, const unsigned char host[32],
                           const mulib_sshserver_policy_t *policy,
                           mulib_sshchannel_slot_t *slots, size_t count)
{
  if (!s) return 0;
  memset (s, 0, sizeof *s);
  if (!host || (!slots && count) || count > SIZE_MAX / sizeof *slots
      || !mulib_sshconnection_version_ (version)
      || (role != MULIB_SSH_CLIENT && role != MULIB_SSH_SERVER)
      || (role == MULIB_SSH_CLIENT ? policy != NULL : !policy || !policy->max_attempts)) return 0;
  if (count) memset (slots, 0, count * sizeof *slots);
  if (policy) s->policy = *policy;
  s->slots = slots;
  s->slot_count = count;
  s->role = role;
  s->phase = MULIB_SSH_CONNECTION_IDENT_;
  s->version_length = version.length;
  memcpy (s->version, version.data, version.length);
  memcpy (s->host, host, sizeof s->host);
  memcpy (s->tx, version.data, version.length);
  memcpy (s->tx + version.length, "\r\n", 2);
  s->tx_end = version.length + 2;
  return 1;
}

mulib_ssh_slice_t
mulib_sshconnection_output (const mulib_sshconnection_t *s)
{
  mulib_ssh_slice_t output = {NULL, 0};
  if (s && s->tx_end > s->tx_start) {
    output.data = s->tx + s->tx_start;
    output.length = s->tx_end - s->tx_start;
  }
  return output;
}

int
mulib_sshconnection_consume_output (mulib_sshconnection_t *s, size_t n)
{
  if (!s || !s->phase || n > s->tx_end - s->tx_start) return 0;
  s->tx_start += n;
  if (s->tx_start == s->tx_end) s->tx_start = s->tx_end = 0;
  return 1;
}

int
mulib_sshconnection_can_auth (const mulib_sshconnection_t *s)
{
  return s && s->role == MULIB_SSH_CLIENT && s->phase == MULIB_SSH_CONNECTION_USERAUTH_
    && !s->pending_auth && !s->auth_exhausted && !s->tx_end && mulib_sshtransport_ready (&s->transport);
}

int
mulib_sshconnection_authenticated (const mulib_sshconnection_t *s)
{
  return s && s->phase == MULIB_SSH_CONNECTION_ACTIVE_;
}

mulib_ssh_slice_t
mulib_sshconnection_user (const mulib_sshconnection_t *s)
{
  mulib_ssh_slice_t user = {NULL, 0};
  if (mulib_sshconnection_authenticated (s)) {
    user.data = s->user;
    user.length = s->user_length;
  }
  return user;
}

int
mulib_sshconnection_rekey_due (const mulib_sshconnection_t *s)
{
  return s && mulib_sshtransport_rekey_due (&s->transport);
}

static int
mulib_sshconnection_queue_ (mulib_sshconnection_t *s, size_t size)
{
  size_t written;
  int ok = mulib_sshtransport_seal (&s->transport, s->encode, size,
                                    s->tx + s->tx_end, sizeof s->tx - s->tx_end, &written);
  mulib_secret_wipe (s->encode, size);
  if (ok != 1) return mulib_sshconnection_fail_ (s);
  s->tx_end += written;
  return 1;
}

int
mulib_sshconnection_auth (mulib_sshconnection_t *s, mulib_sshconnection_auth_kind_t kind,
                          mulib_ssh_slice_t user, mulib_ssh_slice_t credential)
{
  size_t n;
  unsigned char session_id[32];
  int ok = 0;
  if (!mulib_sshconnection_can_auth (s)) return 0;
  switch (kind) {
    case MULIB_SSH_CONNECTION_AUTH_NONE:
      ok = !credential.length && mulib_sshauth_none_write (user, s->encode, sizeof s->encode, &n);
      break;
    case MULIB_SSH_CONNECTION_AUTH_PASSWORD:
      ok = mulib_sshauth_password_write (user, credential, s->encode, sizeof s->encode, &n);
      break;
    case MULIB_SSH_CONNECTION_AUTH_QUERY:
      ok = credential.data && credential.length == 32
        && mulib_sshauth_public_query (user, credential.data, s->encode, sizeof s->encode, &n);
      break;
    case MULIB_SSH_CONNECTION_AUTH_SIGNED:
      ok = credential.data && credential.length == 32
        && mulib_sshtransport_session_id (&s->transport, session_id)
        && mulib_sshauth_public_write (user, credential.data, session_id, s->encode, sizeof s->encode, &n);
      break;
    default:
      break;
  }
  if (!ok) return 0;
  if (mulib_sshconnection_queue_ (s, n) != 1) return -1;
  s->pending_auth = kind;
  if (kind == MULIB_SSH_CONNECTION_AUTH_QUERY)
    memcpy (s->query_key, credential.data, sizeof s->query_key);
  memcpy (s->user, user.data, user.length);
  s->user_length = user.length;
  return 1;
}

int
mulib_sshconnection_banner (mulib_sshconnection_t *s, mulib_ssh_slice_t message,
                            mulib_ssh_slice_t language)
{
  size_t n;
  if (!s || s->role != MULIB_SSH_SERVER || s->phase != MULIB_SSH_CONNECTION_USERAUTH_
      || s->tx_end || !mulib_sshtransport_ready (&s->transport)
      || !mulib_sshauth_banner_write (message, language, s->encode, sizeof s->encode, &n))
    return 0;
  return mulib_sshconnection_queue_ (s, n);
}

int
mulib_sshconnection_channel (mulib_sshconnection_t *s, const mulib_sshchannel_command_t *command)
{
  size_t n;
  if (!mulib_sshconnection_authenticated (s) || s->tx_end || !mulib_sshtransport_ready (&s->transport)
      || !mulib_sshchannel_dispatch_send (s->slots, s->slot_count, command, s->encode, sizeof s->encode, &n))
    return 0;
  return mulib_sshconnection_queue_ (s, n);
}

static int
mulib_sshconnection_ident_ (mulib_sshconnection_t *s, const unsigned char *p, size_t n,
                            size_t *used, mulib_sshconnection_event_t *e)
{
  size_t limit = MULIB_SSH_CONNECTION_IDENT_LINE_MAX_;
  const unsigned char *end = (const unsigned char *)memchr (p, '\n', n < limit ? n : limit);
  if (!end) return n < limit ? 0 : -1;
  size_t len = (size_t)(end - p);
  mulib_ssh_slice_t remote = {p, len && end[-1] == '\r' ? len - 1 : len};
  if (len >= 4 && !memcmp (p, "SSH-", 4)) {
    mulib_ssh_slice_t local = {s->version, s->version_length};
    int client = s->role == MULIB_SSH_CLIENT;
    if (remote.length == len || !mulib_sshconnection_version_ (remote)
        || !mulib_sshtransport_start (&s->transport, s->role, client ? local : remote,
                                       client ? remote : local, s->host,
                                       s->tx, MULIB_SSH_KEX_OUTPUT_MIN, &s->tx_end)) return -1;
    mulib_secret_wipe (s->host, sizeof s->host);
    s->phase = MULIB_SSH_CONNECTION_KEX_;
  } else {
    if (s->role != MULIB_SSH_CLIENT || len + 1 > MULIB_SSH_CONNECTION_BANNERS_MAX_ - s->banner_bytes
        || memchr (p, 0, remote.length)) return -1;
    s->banner_bytes += len + 1;
    e->type = MULIB_SSH_CONNECTION_IDENT_BANNER;
    e->text = remote;
  }
  *used = len + 1;
  return 1;
}

static int
mulib_sshconnection_server_auth_ (mulib_sshconnection_t *s, mulib_ssh_slice_t p,
                                  mulib_sshconnection_event_t *e)
{
  mulib_ssh_auth_request_t request;
  unsigned char sid[32];
  size_t n;
  int ok = 0, query;
  if (p.data[0] != MULIB_SSH_USERAUTH_REQUEST || s->attempts >= s->policy.max_attempts) return 0;
  int status = mulib_sshauth_parse (p.data, p.length, &request);
  if (!status) return 0;
  ++s->attempts;
  query = status == 1 && request.method == MULIB_SSH_AUTH_ED25519 && !request.signature.length;
  if (status == 1 && request.method == MULIB_SSH_AUTH_PASSWORD && s->policy.verify_password)
    ok = s->policy.verify_password (s->policy.context, &request) == 1;
  else if (status == 1 && request.method == MULIB_SSH_AUTH_ED25519 && s->policy.authorize_key) {
    /* Queries authorize only this reply, never a future request. On the last
     * attempt there is no room for a subsequent signed request. */
    if (query ? s->attempts < s->policy.max_attempts
        : mulib_sshtransport_session_id (&s->transport, sid)
          && mulib_sshauth_public_verify (p.data, p.length, sid, &request))
      ok = s->policy.authorize_key (s->policy.context, &request) == 1;
  }
  if (query && ok)
    return mulib_sshauth_pk_ok_write (request.public_key.data, s->encode, sizeof s->encode, &n)
      && mulib_sshconnection_queue_ (s, n) == 1;

  const char *methods = "";
  if (s->attempts < s->policy.max_attempts) {
    if (s->policy.verify_password) methods = s->policy.authorize_key ? "publickey,password" : "password";
    else if (s->policy.authorize_key) methods = "publickey";
  }
  mulib_ssh_slice_t list = {(const unsigned char *)methods, strlen (methods)};
  if (!mulib_sshauth_reply_write (ok, list, 0, s->encode, sizeof s->encode, &n)
      || mulib_sshconnection_queue_ (s, n) != 1) return 0;
  if (ok) {
    memcpy (s->user, request.user.data, request.user.length);
    s->user_length = request.user.length;
    s->phase = MULIB_SSH_CONNECTION_ACTIVE_;
    e->type = MULIB_SSH_CONNECTION_AUTH_REPLY;
    e->auth.type = MULIB_SSH_USERAUTH_SUCCESS;
  }
  return 1;
}

static int
mulib_sshconnection_client_auth_ (mulib_sshconnection_t *s, mulib_ssh_slice_t p,
                                  mulib_sshconnection_event_t *e)
{
  if (p.data[0] == MULIB_SSH_USERAUTH_PK_OK) {
    if (s->pending_auth != MULIB_SSH_CONNECTION_AUTH_QUERY
        || !mulib_sshauth_pk_ok_parse (p.data, p.length, &e->text)
        || memcmp (s->query_key, e->text.data, sizeof s->query_key)) return 0;
    e->type = MULIB_SSH_CONNECTION_AUTH_PK_OK;
  } else {
    if (!mulib_sshauth_reply_parse (p.data, p.length, &e->auth) || e->auth.partial) return 0;
    e->type = MULIB_SSH_CONNECTION_AUTH_REPLY;
    if (e->auth.type == MULIB_SSH_USERAUTH_BANNER) return 1;
    if (!s->pending_auth) return 0;
    if (e->auth.type == MULIB_SSH_USERAUTH_SUCCESS) {
      if (s->pending_auth == MULIB_SSH_CONNECTION_AUTH_QUERY) return 0;
      s->phase = MULIB_SSH_CONNECTION_ACTIVE_;
    } else if (!e->auth.methods.length) s->auth_exhausted = 1;
  }
  if (!mulib_sshconnection_authenticated (s)) {
    mulib_secret_wipe (s->user, s->user_length);
    s->user_length = 0;
  }
  s->pending_auth = (mulib_sshconnection_auth_kind_t)0;
  return 1;
}

static int
mulib_sshconnection_packet_ (mulib_sshconnection_t *s, mulib_ssh_slice_t p,
                              mulib_sshconnection_event_t *e)
{
  unsigned char type = p.data[0], flag;
  size_t n;
  mulib_ssh_reader_t r = {p.data + 1, p.length - 1};
  mulib_ssh_slice_t text;
  if (s->phase == MULIB_SSH_CONNECTION_SERVICE_) {
    int client = s->role == MULIB_SSH_CLIENT;
    if (!mulib_sshauth_service_parse (client, p.data, p.length)) return 0;
    if (!client && (!mulib_sshauth_service_write (1, s->encode, sizeof s->encode, &n)
                    || mulib_sshconnection_queue_ (s, n) != 1)) return 0;
    s->phase = MULIB_SSH_CONNECTION_USERAUTH_;
    e->type = MULIB_SSH_CONNECTION_AUTH_SERVICE;
    return 1;
  }
  if (s->phase == MULIB_SSH_CONNECTION_USERAUTH_)
    return s->role == MULIB_SSH_CLIENT ? mulib_sshconnection_client_auth_ (s, p, e)
      : mulib_sshconnection_server_auth_ (s, p, e);
  /* RFC 4252 requires ignoring additional auth requests after success. */
  if (s->role == MULIB_SSH_SERVER && type == MULIB_SSH_USERAUTH_REQUEST) return 1;
  if (type == MULIB_SSH_CONNECTION_GLOBAL_REQUEST_) {
    /* No global requests are implemented, but peers can probe for them. */
    if (!mulib_ssh_read_string (&r, &text) || !text.length || !mulib_ssh_read_u8 (&r, &flag)) return 0;
    if (!flag) return 1;
    s->encode[0] = MULIB_SSH_CONNECTION_REQUEST_FAILURE_;
    return mulib_sshconnection_queue_ (s, 1) == 1;
  }
  if (!mulib_sshchannel_dispatch_receive (s->slots, s->slot_count, p.data, p.length, &e->channel))
    return 0;
  if (e->channel.slot == SIZE_MAX) {
    mulib_ssh_slice_t empty = {NULL, 0};
    if (!mulib_sshchannel_failure_write (e->channel.endpoint.id, e->channel.message.value, empty,
                                         s->encode, sizeof s->encode, &n)
        || mulib_sshconnection_queue_ (s, n) != 1) return 0;
  }
  e->type = MULIB_SSH_CONNECTION_CHANNEL;
  return 1;
}

int
mulib_sshconnection_feed (mulib_sshconnection_t *s, const void *data, size_t size,
                          size_t *used, mulib_sshconnection_event_t *out)
{
  mulib_sshconnection_event_t event;
  size_t consumed;
  int status;
  if (used) *used = 0;
  if (!s || !s->phase) return -1;
  if (!used || !out || (!data && size)) return mulib_sshconnection_fail_ (s);
  if (s->plain_used) {
    mulib_secret_wipe (s->plain, s->plain_used);
    s->plain_used = 0;
  }
  if (s->rx_start && size > sizeof s->rx - s->rx_end) {
    s->rx_end -= s->rx_start;
    memmove (s->rx, s->rx + s->rx_start, s->rx_end);
    s->rx_start = 0;
  }
  size_t room = sizeof s->rx - s->rx_end;
  *used = size < room ? size : room;
  if (*used) {
    memcpy (s->rx + s->rx_end, data, *used);
    s->rx_end += *used;
  }
  if (s->tx_end || s->rx_start == s->rx_end) return 0;
  const unsigned char *p = s->rx + s->rx_start;
  size_t length = s->rx_end - s->rx_start;
  memset (&event, 0, sizeof event);
  if (s->phase == MULIB_SSH_CONNECTION_IDENT_)
    status = mulib_sshconnection_ident_ (s, p, length, &consumed, &event);
  else {
    mulib_ssh_slice_t payload;
    status = mulib_sshtransport_receive (&s->transport, p, length, s->plain, sizeof s->plain,
                                         &payload, &consumed, s->tx, MULIB_SSH_KEX_OUTPUT_MIN, &s->tx_end);
    if (status == 1 && s->phase == MULIB_SSH_CONNECTION_KEX_ && mulib_sshtransport_ready (&s->transport)) {
      size_t n;
      s->phase = MULIB_SSH_CONNECTION_SERVICE_;
      if (s->role == MULIB_SSH_CLIENT
          && (!mulib_sshauth_service_write (0, s->encode, sizeof s->encode, &n)
              || mulib_sshconnection_queue_ (s, n) != 1)) goto fail;
    }
    if (status == 1 && payload.length) {
      size_t plain = (size_t)mulib_endian_load_be32 (p);
      if (s->phase == MULIB_SSH_CONNECTION_KEX_ || !mulib_sshconnection_packet_ (s, payload, &event))
        goto fail;
      if (event.type == MULIB_SSH_CONNECTION_NONE || event.type == MULIB_SSH_CONNECTION_AUTH_SERVICE
          || (event.type == MULIB_SSH_CONNECTION_AUTH_REPLY && s->role == MULIB_SSH_SERVER))
        mulib_secret_wipe (s->plain, plain);
      else
        s->plain_used = plain;
    }
  }
  if (!status && length < sizeof s->rx) return 0;
  if (status != 1) goto fail;
  s->rx_start += consumed;
  if (s->rx_start == s->rx_end) s->rx_start = s->rx_end = 0;
  *out = event;
  return 1;

fail:
  *used = 0;
  return mulib_sshconnection_fail_ (s);
}
#endif
