/** @file mulib-sshtransport.h
 * @brief Socket-independent SSH-2 key exchange, rekey and encrypted transport.
 * Define MULIB_IMPL_SSHTRANSPORT plus the SSHKEX and SSHPACKET implementations.
 */
#ifndef MULIB_SSHTRANSPORT_H
#define MULIB_SSHTRANSPORT_H
#include "mulib-sshkex.h"
#include "mulib-sshpacket.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {
  MULIB_SSH_KEX_OUTPUT_MIN = 512, MULIB_SSH_KEXINIT_MAX = 8192,
  MULIB_SSH_IDENT_MAX = 253
};
typedef enum {
  MULIB_SSH_CLIENT = 1, MULIB_SSH_SERVER = 2
} mulib_ssh_role_t;

typedef struct {
  /* Private state. Callers own storage and must serialize access. */
  mulib_ssh_role_t role;
  unsigned stage, skip_guess, established;
  unsigned char host[32], scalar[32], ephemeral[32], session_id[32];
  unsigned char versions[2][MULIB_SSH_IDENT_MAX];
  size_t version_length[2];
  unsigned char local_init[MULIB_SSH_KEX_OUTPUT_MIN], peer_init[MULIB_SSH_KEXINIT_MAX];
  size_t local_init_length, peer_init_length;
  mulib_ssh_gcm_t tx, rx, next_rx;
} mulib_sshtransport_t;

/* Versions are exact wire text without CRLF (including any comments).
 * Client host material is a mandatory trusted raw Ed25519 public key pin.
 * Server host material is its Ed25519 seed. Init retains the host material
 * and versions for rekey and emits the first cleartext frame. Always queue
 * emitted bytes in order and transmit exactly once, retaining partial writes.
 * Output may contain multiple frames. All buffers must be disjoint from one
 * another and from transport state. Clear the state at teardown. */
int mulib_sshtransport_start (mulib_sshtransport_t *, mulib_ssh_role_t,
                              mulib_ssh_slice_t client_version, mulib_ssh_slice_t server_version,
                              const unsigned char host[32], void *, size_t, size_t *written);
/* Consume one clear initial handshake frame and emit zero or more replies.
 * Output capacity must be at least MULIB_SSH_KEX_OUTPUT_MIN. Returns 1 on
 * progress, 0 for incomplete input, -2 for short output, -1 on fatal error.
 * used and written are zero except on progress. On fatal error state is wiped. */
int mulib_sshtransport_handshake (mulib_sshtransport_t *, const void *, size_t,
                                  size_t *used, void *, size_t, size_t *written);
/* Both NEWKEYS directions are complete and no exchange is in progress. */
int mulib_sshtransport_ready (const mulib_sshtransport_t *);
/* Copy the first exchange hash after the initial handshake completes (1/0).
 * It remains available and unchanged during and after rekey. */
int mulib_sshtransport_session_id (const mulib_sshtransport_t *, unsigned char out[32]);
/* Initiate rekey by sending KEXINIT under the current outbound key. Requires
 * MULIB_SSH_KEX_OUTPUT_MIN bytes of output capacity. Returns 1, 0 if a rekey
 * is already in progress, -2 for short output, -1 fatal. Use receive (not
 * open) to continue the exchange. Strict KEX is not negotiated. */
int mulib_sshtransport_rekey (mulib_sshtransport_t *, void *, size_t, size_t *written);
/* Soft advisory for either direction's current AEAD budget; rekey promptly,
 * before hard limits cause a fatal return. Returns 0 until initially ready. */
int mulib_sshtransport_rekey_due (const mulib_sshtransport_t *);
/* Unified receive for initial exchange and all encrypted traffic. Consumes
 * one record, returns 1 with either an authenticated application payload
 * borrowing scratch, or an empty slice for internally handled messages.
 * Queue any emitted replies before sending further records. For a complete
 * record, reply capacity must be at least MULIB_SSH_KEX_OUTPUT_MIN; scratch
 * is only needed for encrypted records (MULIB_SSH_BODY_MAX accepts all).
 * Returns 0 incomplete, -2 short scratch/reply (retry unchanged), -1 fatal.
 * used and written are zero except on progress; payload is only set on 1.
 * IGNORE, DEBUG and UNIMPLEMENTED are handled internally. A disconnect is
 * fatal. Close on -1; wipe application scratch after use. */
int mulib_sshtransport_receive (mulib_sshtransport_t *, const void *, size_t,
                                void *scratch, size_t, mulib_ssh_slice_t *payload,
                                size_t *used, void *replies, size_t, size_t *written);
/* Seal application packets after the initial exchange. Returns 1, -2 for
 * short output, -3 if rekey has blocked application sends (retry after
 * local NEWKEYS is queued), and -1 invalid/fatal. */
int mulib_sshtransport_seal (mulib_sshtransport_t *, const void *, size_t,
                             void *, size_t, size_t *written);
/* Legacy application-only receive when ready. For rekey support always use
 * receive instead. Returns 1, 0 incomplete, -2 short scratch, -1 fatal.
 * Close on -1; wipe scratch promptly after processing private payloads. */
int mulib_sshtransport_open (mulib_sshtransport_t *, const void *, size_t,
                             void *scratch, size_t, mulib_ssh_slice_t *, size_t *used);
void mulib_sshtransport_clear (mulib_sshtransport_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SSHTRANSPORT) && !defined(MULIB_SSHTRANSPORT_IMPLEMENTED)
#define MULIB_SSHTRANSPORT_IMPLEMENTED
#include <string.h>

enum {
  MULIB_SSH_TRANSPORT_KEXINIT_ = 1, MULIB_SSH_TRANSPORT_ECDH_,
  MULIB_SSH_TRANSPORT_NEWKEYS_, MULIB_SSH_TRANSPORT_READY_
};

static int
mulib_sshtransport_random_ (void *context, void *out, size_t n)
{
  (void)context;
  return mulib_entropy (out, n);
}

static int
mulib_sshtransport_version_ (mulib_ssh_slice_t v)
{
  if (!v.data || v.length < 9 || v.length > MULIB_SSH_IDENT_MAX
      || memcmp (v.data, "SSH-2.0-", 8) || v.data[8] == ' ') return 0;
  for (size_t i = 8; i < v.length; ++i)
    if (v.data[i] < 32 || v.data[i] > 126) return 0;
  return 1;
}

void
mulib_sshtransport_clear (mulib_sshtransport_t *s)
{
  if (s) mulib_secret_wipe (s, sizeof *s);
}

static int
mulib_sshtransport_fail_ (mulib_sshtransport_t *s)
{
  mulib_sshtransport_clear (s);
  return -1;
}

/* Send with the current outbound keys, or cleartext before local NEWKEYS.
 * Return 1, -2 for insufficient space, -1 on unrecoverable error. */
static int
mulib_sshtransport_emit_ (mulib_sshtransport_t *s, const void *payload, size_t length,
                          unsigned char *out, size_t cap, size_t *used)
{
  int encrypted = s->tx.active;
  size_t body = mulib_sshpacket_body_size (length, encrypted), frame;
  *used = 0;
  if (!body || !payload) return -1;
  size_t total = body + 4 + (encrypted ? 16 : 0);
  if (cap < total) return -2;
  if (!out) return -1;
  if (!mulib_sshpacket_write (payload, length, encrypted, out, cap, &frame,
                              mulib_sshtransport_random_, NULL)
      || (encrypted && !mulib_ssh_gcm_seal (&s->tx, out, out + 4, body, out + frame))) {
    mulib_secret_wipe (out, total);
    return -1;
  }
  *used = total;
  return 1;
}

static int
mulib_sshtransport_propose_ (mulib_sshtransport_t *s)
{
  unsigned char cookie[16];
  return mulib_entropy (cookie, sizeof cookie)
         && mulib_sshkex_propose (cookie, s->local_init, sizeof s->local_init,
                                  &s->local_init_length);
}

static int
mulib_sshtransport_ephemeral_ (mulib_sshtransport_t *s)
{
  return mulib_entropy (s->scalar, sizeof s->scalar)
         && mulib_x25519_public (s->ephemeral, s->scalar);
}

int
mulib_sshtransport_start (mulib_sshtransport_t *s, mulib_ssh_role_t role,
                          mulib_ssh_slice_t vc, mulib_ssh_slice_t vs,
                          const unsigned char host[32], void *out, size_t cap, size_t *written)
{
  if (written) *written = 0;
  if (!s) return 0;
  mulib_sshtransport_clear (s);
  if (!host || !out || !written || (role != MULIB_SSH_CLIENT && role != MULIB_SSH_SERVER)
      || !mulib_sshtransport_version_ (vc) || !mulib_sshtransport_version_ (vs)
      || !mulib_sshtransport_propose_ (s)
      || mulib_sshtransport_emit_ (s, s->local_init, s->local_init_length,
                                  out, cap, written) != 1) {
    mulib_sshtransport_clear (s);
    return 0;
  }
  s->role = role;
  s->stage = MULIB_SSH_TRANSPORT_KEXINIT_;
  memcpy (s->host, host, sizeof s->host);
  memcpy (s->versions[0], vc.data, vc.length);
  memcpy (s->versions[1], vs.data, vs.length);
  s->version_length[0] = vc.length;
  s->version_length[1] = vs.length;
  return 1;
}

int
mulib_sshtransport_rekey (mulib_sshtransport_t *s, void *out, size_t cap, size_t *written)
{
  if (written) *written = 0;
  if (!s || !s->established || !written) return mulib_sshtransport_fail_ (s);
  if (!mulib_sshtransport_ready (s)) return 0;
  if (cap < MULIB_SSH_KEX_OUTPUT_MIN) return -2;
  if (!out || !mulib_sshtransport_propose_ (s)
      || mulib_sshtransport_emit_ (s, s->local_init, s->local_init_length,
                                  out, cap, written) != 1)
    return mulib_sshtransport_fail_ (s);
  s->stage = MULIB_SSH_TRANSPORT_KEXINIT_;
  return 1;
}

/* Move rather than retain a second copy of active nonce/counter state. */
static void
mulib_sshtransport_activate_ (mulib_ssh_gcm_t *current, mulib_ssh_gcm_t *next)
{
  mulib_ssh_gcm_clear (current);
  *current = *next;
  mulib_ssh_gcm_clear (next);
}

static int
mulib_sshtransport_derive_ (mulib_sshtransport_t *s, const unsigned char secret[32],
                           const unsigned char hash[32], mulib_ssh_gcm_t *next_tx)
{
  unsigned char iv[12], key[16];
  int client = s->role == MULIB_SSH_CLIENT;
  int ok = 1;
  const unsigned char *sid = s->established ? s->session_id : hash;
  mulib_ssh_gcm_t *directions[2] = {client ? next_tx : &s->next_rx, client ? &s->next_rx : next_tx};
  for (unsigned i = 0; ok && i < 2; ++i)
    ok = mulib_ssh_derive (secret, hash, sid, (unsigned char)('A' + i), iv, sizeof iv)
         && mulib_ssh_derive (secret, hash, sid, (unsigned char)('C' + i), key, sizeof key)
         && mulib_ssh_gcm_init (directions[i], key, iv);
  mulib_secret_wipe (key, sizeof key);
  mulib_secret_wipe (iv, sizeof iv);
  return ok;
}

static int
mulib_sshtransport_exchange_ (mulib_sshtransport_t *s, mulib_ssh_slice_t payload,
                              unsigned char *out, size_t cap, size_t *written)
{
  unsigned char host[32], secret[32], hash[32], signature[64], reply[MULIB_SSH_ECDH_REPLY_SIZE];
  mulib_ssh_gcm_t next_tx = {0};
  mulib_ssh_slice_t peer, peer_host, peer_signature;
  mulib_ssh_slice_t local = {s->local_init, s->local_init_length};
  mulib_ssh_slice_t remote = {s->peer_init, s->peer_init_length};
  int client = s->role == MULIB_SSH_CLIENT;
  mulib_ssh_transcript_t transcript = {
    {s->versions[0], s->version_length[0]}, {s->versions[1], s->version_length[1]},
    client ? local : remote, client ? remote : local
  };
  size_t first = 0, second, reply_length;
  unsigned char newkeys = MULIB_SSH_NEWKEYS;
  int ok;
  if (client) {
    ok = mulib_sshkex_reply_parse (payload.data, payload.length, &peer_host, &peer, &peer_signature)
         && mulib_secret_equal (peer_host.data, s->host, sizeof host);
    if (ok) memcpy (host, peer_host.data, sizeof host);
  } else {
    ok = mulib_sshkex_init_parse (payload.data, payload.length, &peer)
         && mulib_ed25519_public (host, s->host);
  }
  ok = ok && mulib_x25519 (secret, s->scalar, peer.data)
       && mulib_sshkex_hash (&transcript, host, client ? s->ephemeral : peer.data,
                             client ? peer.data : s->ephemeral, secret, hash);
  if (ok && client)
    ok = mulib_ed25519_verify (peer_signature.data, host, hash, sizeof hash);
  ok = ok && mulib_sshtransport_derive_ (s, secret, hash, &next_tx);
  if (ok && !client)
    ok = mulib_ed25519_sign (signature, s->host, hash, sizeof hash)
         && mulib_sshkex_reply_write (host, s->ephemeral, signature, reply, sizeof reply, &reply_length)
         && mulib_sshtransport_emit_ (s, reply, reply_length, out, cap, &first) == 1;
  ok = ok && mulib_sshtransport_emit_ (s, &newkeys, 1, out + first, cap - first, &second) == 1;
  mulib_secret_wipe (secret, sizeof secret);
  mulib_secret_wipe (s->scalar, sizeof s->scalar);
  if (ok) {
    if (!s->established) memcpy (s->session_id, hash, sizeof s->session_id);
    mulib_sshtransport_activate_ (&s->tx, &next_tx);
    *written = first + second;
  } else {
    mulib_ssh_gcm_clear (&next_tx);
    mulib_secret_wipe (out, first);
  }
  mulib_secret_wipe (hash, sizeof hash);
  return ok;
}

static int
mulib_sshtransport_kexinit_ (mulib_sshtransport_t *s, mulib_ssh_slice_t payload,
                             unsigned char *out, size_t cap, size_t *written)
{
  size_t first = 0, second = 0;
  int skip;
  if (payload.length > sizeof s->peer_init
      || !mulib_sshkex_negotiate (payload.data, payload.length, &skip)
      || !mulib_sshtransport_ephemeral_ (s))
    return 0;
  if (s->stage == MULIB_SSH_TRANSPORT_READY_
      && (!mulib_sshtransport_propose_ (s)
          || mulib_sshtransport_emit_ (s, s->local_init, s->local_init_length,
                                      out, cap, &first) != 1))
    return 0;
  if (s->role == MULIB_SSH_CLIENT) {
    unsigned char init[MULIB_SSH_ECDH_INIT_SIZE];
    size_t length;
    if (!mulib_sshkex_init_write (s->ephemeral, init, sizeof init, &length)
        || mulib_sshtransport_emit_ (s, init, length, out + first, cap - first, &second) != 1) {
      mulib_secret_wipe (out, first);
      return 0;
    }
  }
  memcpy (s->peer_init, payload.data, payload.length);
  s->peer_init_length = payload.length;
  s->skip_guess = (unsigned)skip;
  s->stage = MULIB_SSH_TRANSPORT_ECDH_;
  *written = first + second;
  return 1;
}

static int
mulib_sshtransport_is_kex_ (unsigned char type)
{
  return type >= 20 && type < 50;
}

static int
mulib_sshtransport_is_generic_ (unsigned char type)
{
  return type == MULIB_SSH_IGNORE || type == MULIB_SSH_DEBUG || type == MULIB_SSH_UNIMPLEMENTED;
}

/* Return 1 for internal progress, 2 for application data, or -1 fatal. */
static int
mulib_sshtransport_dispatch_ (mulib_sshtransport_t *s, mulib_ssh_slice_t payload,
                              void *out, size_t cap, size_t *written)
{
  unsigned char type = payload.data[0];
  if (type == MULIB_SSH_DISCONNECT) return -1;
  if (s->skip_guess) {
    s->skip_guess = 0;
    return 1;
  }
  /* Non-strict KEX allows transport-generic messages during exchanges. */
  if (mulib_sshtransport_is_generic_ (type)) return 1;
  if (type == MULIB_SSH_KEXINIT
      && (s->stage == MULIB_SSH_TRANSPORT_READY_ || s->stage == MULIB_SSH_TRANSPORT_KEXINIT_))
    return mulib_sshtransport_kexinit_ (s, payload, out, cap, written) ? 1 : -1;
  if (s->stage == MULIB_SSH_TRANSPORT_READY_
      || (s->established && s->stage == MULIB_SSH_TRANSPORT_KEXINIT_)) {
    /* Before the peer's KEXINIT, old application records can still arrive. */
    return mulib_sshtransport_is_kex_ (type) ? -1 : 2;
  }
  /* No extensions are negotiated; application/service messages are not
   * allowed between the peer's KEXINIT and NEWKEYS. */
  if (s->stage == MULIB_SSH_TRANSPORT_ECDH_) {
    if (!mulib_sshtransport_exchange_ (s, payload, out, cap, written)) return -1;
    s->stage = MULIB_SSH_TRANSPORT_NEWKEYS_;
    return 1;
  }
  if (s->stage == MULIB_SSH_TRANSPORT_NEWKEYS_ && type == MULIB_SSH_NEWKEYS
      && payload.length == 1) {
    mulib_sshtransport_activate_ (&s->rx, &s->next_rx);
    s->local_init_length = s->peer_init_length = 0;
    s->established = 1;
    s->stage = MULIB_SSH_TRANSPORT_READY_;
    return 1;
  }
  return -1;
}

int
mulib_sshtransport_ready (const mulib_sshtransport_t *s)
{
  return s && s->stage == MULIB_SSH_TRANSPORT_READY_;
}

static int
mulib_sshtransport_budget_low_ (const mulib_ssh_gcm_t *g)
{
  /* Leave headroom for in-flight application records and exchange frames. */
  return g->packets >= MULIB_SSH_GCM_PACKET_LIMIT - 8192
         || g->bytes >= MULIB_SSH_GCM_BYTE_LIMIT - (UINT64_C (64) << 20)
         || mulib_endian_load_be64 (g->nonce + 4) >= UINT64_MAX - 8192;
}

int
mulib_sshtransport_rekey_due (const mulib_sshtransport_t *s)
{
  return s && s->established
         && (mulib_sshtransport_budget_low_ (&s->tx) || mulib_sshtransport_budget_low_ (&s->rx));
}

int
mulib_sshtransport_session_id (const mulib_sshtransport_t *s, unsigned char out[32])
{
  if (!out || !s || !s->established) return 0;
  memcpy (out, s->session_id, sizeof s->session_id);
  return 1;
}

int
mulib_sshtransport_seal (mulib_sshtransport_t *s, const void *data, size_t n,
                         void *output, size_t cap, size_t *written)
{
  const unsigned char *p = (const unsigned char *)data;
  if (written) *written = 0;
  if (!s || !s->established || !p || !n || n > MULIB_SSH_PAYLOAD_MAX || !written
      || mulib_sshtransport_is_kex_ (p[0]))
    return mulib_sshtransport_fail_ (s);
  if (s->stage != MULIB_SSH_TRANSPORT_READY_ && s->stage != MULIB_SSH_TRANSPORT_NEWKEYS_
      && p[0] != MULIB_SSH_DISCONNECT && !mulib_sshtransport_is_generic_ (p[0])) return -3;
  int status = mulib_sshtransport_emit_ (s, p, n, output, cap, written);
  return status == -1 ? mulib_sshtransport_fail_ (s) : status;
}

static int
mulib_sshtransport_receive_ (mulib_sshtransport_t *s, const void *data, size_t n,
                             void *scratch, size_t cap, mulib_ssh_slice_t *out, size_t *used,
                             void *replies, size_t reply_cap, size_t *written)
{
  const unsigned char *p = (const unsigned char *)data;
  mulib_ssh_slice_t payload;
  size_t body, frame;
  if (used) *used = 0;
  if (written) *written = 0;
  if (!s || !s->stage || !out || !used || (!written && !mulib_sshtransport_ready (s)))
    return mulib_sshtransport_fail_ (s);
  int encrypted = s->rx.active;
  int status = mulib_sshpacket_peek (p, n, encrypted, &body, &frame);
  if (status < 0) return mulib_sshtransport_fail_ (s);
  if (!status) return 0;
  if ((written && reply_cap < MULIB_SSH_KEX_OUTPUT_MIN) || (encrypted && cap < body)) return -2;
  if ((written && !replies) || (encrypted && !scratch)) return mulib_sshtransport_fail_ (s);
  if (encrypted) {
    memcpy (scratch, p + 4, body);
    if (!mulib_ssh_gcm_open (&s->rx, p, scratch, body, p + 4 + body)) goto fail;
  }
  if (!mulib_sshpacket_payload (encrypted ? scratch : p + 4, body, &payload)) goto fail;
  status = written ? mulib_sshtransport_dispatch_ (s, payload, replies, reply_cap, written)
                   : (mulib_sshtransport_is_kex_ (payload.data[0]) ? -1 : 2);
  if (status < 0) goto fail;
  if (status == 2) *out = payload;
  else {
    if (encrypted) mulib_secret_wipe (scratch, body);
    out->data = NULL;
    out->length = 0;
  }
  *used = frame;
  return 1;

fail:
  if (encrypted) mulib_secret_wipe (scratch, body);
  return mulib_sshtransport_fail_ (s);
}

int
mulib_sshtransport_handshake (mulib_sshtransport_t *s, const void *data, size_t n,
                              size_t *used, void *out, size_t cap, size_t *written)
{
  mulib_ssh_slice_t ignored;
  if (!written || (s && s->established)) {
    if (used) *used = 0;
    if (written) *written = 0;
    return mulib_sshtransport_fail_ (s);
  }
  return mulib_sshtransport_receive_ (s, data, n, NULL, 0, &ignored, used, out, cap, written);
}

int
mulib_sshtransport_receive (mulib_sshtransport_t *s, const void *data, size_t n,
                             void *scratch, size_t cap, mulib_ssh_slice_t *out,
                             size_t *used, void *replies, size_t reply_cap, size_t *written)
{
  if (!written) {
    if (used) *used = 0;
    return mulib_sshtransport_fail_ (s);
  }
  return mulib_sshtransport_receive_ (s, data, n, scratch, cap, out, used,
                                      replies, reply_cap, written);
}

int
mulib_sshtransport_open (mulib_sshtransport_t *s, const void *data, size_t n,
                          void *scratch, size_t cap, mulib_ssh_slice_t *out, size_t *used)
{
  return mulib_sshtransport_receive_ (s, data, n, scratch, cap, out, used, NULL, 0, NULL);
}
#endif
