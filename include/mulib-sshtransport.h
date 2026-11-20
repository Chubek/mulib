/** @file mulib-sshtransport.h
 * @brief Socket-independent initial SSH-2 key exchange and encrypted transport.
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
  unsigned stage, skip_guess;
  unsigned char host[32], scalar[32], ephemeral[32], session_id[32];
  unsigned char versions[2][MULIB_SSH_IDENT_MAX];
  size_t version_length[2];
  unsigned char local_init[MULIB_SSH_KEX_OUTPUT_MIN], peer_init[MULIB_SSH_KEXINIT_MAX];
  size_t local_init_length, peer_init_length;
  mulib_ssh_gcm_t tx, rx;
} mulib_sshtransport_t;

/* Versions are exact wire text without CRLF (including any comments).
 * Client host material is a mandatory trusted raw Ed25519 public key pin.
 * Server host material is its Ed25519 seed. Init copies inputs and emits the
 * first cleartext frame. Always transmit emitted bytes in order and exactly
 * once. Output may contain multiple frames. All buffers must be disjoint
 * from one another and from transport state. */
int mulib_sshtransport_start (mulib_sshtransport_t *, mulib_ssh_role_t,
                              mulib_ssh_slice_t client_version, mulib_ssh_slice_t server_version,
                              const unsigned char host[32], void *, size_t, size_t *written);
/* Consume one clear handshake frame and emit zero or more reply frames.
 * Output capacity must be at least MULIB_SSH_KEX_OUTPUT_MIN. Returns 1 on
 * progress, 0 for incomplete input, -2 for short output, -1 on fatal error.
 * used and written are zero except on progress. On fatal error state is wiped. */
int mulib_sshtransport_handshake (mulib_sshtransport_t *, const void *, size_t,
                                  size_t *used, void *, size_t, size_t *written);
int mulib_sshtransport_ready (const mulib_sshtransport_t *);
/* Copy session identifier to out after the handshake completes (1/0). */
int mulib_sshtransport_session_id (const mulib_sshtransport_t *, unsigned char out[32]);
/* Application packet I/O after handshake. Buffers are nonoverlapping. Seal
 * returns 1, -2 for short output, and -1 for an invalid call or fatal error.
 * Open returns 1, 0 incomplete, -2 short scratch, -1 invalid/fatal. A
 * successful open borrows authenticated payload from caller-owned scratch
 * (capacity MULIB_SSH_BODY_MAX accepts every supported frame).
 * Close on -1; wipe scratch promptly after processing private payloads. */
int mulib_sshtransport_seal (mulib_sshtransport_t *, const void *, size_t,
                             void *, size_t, size_t *written);
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

static int
mulib_sshtransport_plain_ (const void *payload, size_t length,
                           void *out, size_t cap, size_t *used)
{
  return mulib_sshpacket_write (payload, length, 0, out, cap, used,
                                mulib_sshtransport_random_, NULL);
}

int
mulib_sshtransport_start (mulib_sshtransport_t *s, mulib_ssh_role_t role,
                          mulib_ssh_slice_t vc, mulib_ssh_slice_t vs,
                          const unsigned char host[32], void *out, size_t cap, size_t *written)
{
  unsigned char cookie[16];
  if (written) *written = 0;
  if (!s) return 0;
  mulib_sshtransport_clear (s);
  if (!host || !out || !written || (role != MULIB_SSH_CLIENT && role != MULIB_SSH_SERVER)
      || !mulib_sshtransport_version_ (vc) || !mulib_sshtransport_version_ (vs)
      || !mulib_entropy (cookie, sizeof cookie)
      || !mulib_entropy (s->scalar, sizeof s->scalar)
      || !mulib_x25519_public (s->ephemeral, s->scalar)
      || !mulib_sshkex_propose (cookie, s->local_init, sizeof s->local_init, &s->local_init_length)
      || !mulib_sshtransport_plain_ (s->local_init, s->local_init_length, out, cap, written)) {
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

static int
mulib_sshtransport_install_ (mulib_sshtransport_t *s, const unsigned char secret[32])
{
  unsigned char iv[12], key[16];
  int client = s->role == MULIB_SSH_CLIENT;
  int ok = 1;
  mulib_ssh_gcm_t *directions[2] = {client ? &s->tx : &s->rx, client ? &s->rx : &s->tx};
  for (unsigned i = 0; ok && i < 2; ++i)
    ok = mulib_ssh_derive (secret, s->session_id, s->session_id, (unsigned char)('A' + i), iv, sizeof iv)
         && mulib_ssh_derive (secret, s->session_id, s->session_id, (unsigned char)('C' + i), key, sizeof key)
         && mulib_ssh_gcm_init (directions[i], key, iv);
  mulib_secret_wipe (key, sizeof key);
  mulib_secret_wipe (iv, sizeof iv);
  return ok;
}

static int
mulib_sshtransport_exchange_ (mulib_sshtransport_t *s, mulib_ssh_slice_t payload,
                              unsigned char *out, size_t cap, size_t *written)
{
  unsigned char host[32], secret[32], signature[64], reply[MULIB_SSH_ECDH_REPLY_SIZE];
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
                            client ? peer.data : s->ephemeral, secret, s->session_id);
  if (ok && client)
    ok = mulib_ed25519_verify (peer_signature.data, host, s->session_id, sizeof s->session_id);
  if (ok && !client)
    ok = mulib_ed25519_sign (signature, s->host, s->session_id, sizeof s->session_id)
         && mulib_sshkex_reply_write (host, s->ephemeral, signature, reply, sizeof reply, &reply_length)
         && mulib_sshtransport_plain_ (reply, reply_length, out, cap, &first);
  ok = ok && mulib_sshtransport_install_ (s, secret)
       && mulib_sshtransport_plain_ (&newkeys, 1, out + first, cap - first, &second);
  mulib_secret_wipe (secret, sizeof secret);
  mulib_secret_wipe (s->scalar, sizeof s->scalar);
  mulib_secret_wipe (s->host, sizeof s->host);
  if (ok) *written = first + second;
  return ok;
}

int
mulib_sshtransport_handshake (mulib_sshtransport_t *s, const void *data, size_t n,
                              size_t *used, void *output, size_t cap, size_t *written)
{
  const unsigned char *p = (const unsigned char *)data;
  unsigned char *out = (unsigned char *)output;
  mulib_ssh_slice_t payload;
  size_t body, frame;
  if (used) *used = 0;
  if (written) *written = 0;
  if (!s || !used || !written || !s->stage || s->stage >= MULIB_SSH_TRANSPORT_READY_)
    return mulib_sshtransport_fail_ (s);
  int status = mulib_sshpacket_peek (p, n, 0, &body, &frame);
  if (status < 0) return mulib_sshtransport_fail_ (s);
  if (!status) return 0;
  if (cap < MULIB_SSH_KEX_OUTPUT_MIN) return -2;
  if (!out || !mulib_sshpacket_payload (p + 4, body, &payload))
    return mulib_sshtransport_fail_ (s);
  if (payload.data[0] == MULIB_SSH_DISCONNECT) return mulib_sshtransport_fail_ (s);
  if (s->skip_guess) {
    s->skip_guess = 0;
  } else if (s->stage == MULIB_SSH_TRANSPORT_KEXINIT_) {
    int skip;
    if (payload.length > sizeof s->peer_init
        || !mulib_sshkex_negotiate (payload.data, payload.length, &skip))
      return mulib_sshtransport_fail_ (s);
    if (s->role == MULIB_SSH_CLIENT) {
      unsigned char init[MULIB_SSH_ECDH_INIT_SIZE];
      size_t length;
      if (!mulib_sshkex_init_write (s->ephemeral, init, sizeof init, &length)
          || !mulib_sshtransport_plain_ (init, length, out, cap, written))
        return mulib_sshtransport_fail_ (s);
    }
    memcpy (s->peer_init, payload.data, payload.length);
    s->peer_init_length = payload.length;
    s->skip_guess = (unsigned)skip;
    s->stage = MULIB_SSH_TRANSPORT_ECDH_;
  } else if (s->stage == MULIB_SSH_TRANSPORT_ECDH_) {
    if (!mulib_sshtransport_exchange_ (s, payload, out, cap, written))
      return mulib_sshtransport_fail_ (s);
    s->stage = MULIB_SSH_TRANSPORT_NEWKEYS_;
  } else {
    if (payload.length != 1 || payload.data[0] != MULIB_SSH_NEWKEYS)
      return mulib_sshtransport_fail_ (s);
    s->stage = MULIB_SSH_TRANSPORT_READY_;
  }
  *used = frame;
  return 1;
}

int
mulib_sshtransport_ready (const mulib_sshtransport_t *s)
{
  return s && s->stage == MULIB_SSH_TRANSPORT_READY_;
}

int
mulib_sshtransport_session_id (const mulib_sshtransport_t *s, unsigned char out[32])
{
  if (!out || !mulib_sshtransport_ready (s)) return 0;
  memcpy (out, s->session_id, sizeof s->session_id);
  return 1;
}

int
mulib_sshtransport_seal (mulib_sshtransport_t *s, const void *data, size_t n,
                         void *output, size_t cap, size_t *written)
{
  const unsigned char *p = (const unsigned char *)data;
  unsigned char *out = (unsigned char *)output;
  size_t body = mulib_sshpacket_body_size (n, 1), length;
  if (written) *written = 0;
  if (!mulib_sshtransport_ready (s) || !body || !p || !written
      || p[0] == MULIB_SSH_KEXINIT || p[0] == MULIB_SSH_NEWKEYS)
    return mulib_sshtransport_fail_ (s);
  if (cap < body + 20) return -2;
  if (!out || !mulib_sshpacket_write (p, n, 1, out, cap, &length,
                                     mulib_sshtransport_random_, NULL))
    return mulib_sshtransport_fail_ (s);
  if (!mulib_ssh_gcm_seal (&s->tx, out, out + 4, body, out + length)) {
    mulib_secret_wipe (out, length);
    return mulib_sshtransport_fail_ (s);
  }
  *written = length + 16;
  return 1;
}

int
mulib_sshtransport_open (mulib_sshtransport_t *s, const void *data, size_t n,
                         void *scratch, size_t cap, mulib_ssh_slice_t *out, size_t *used)
{
  const unsigned char *p = (const unsigned char *)data;
  mulib_ssh_slice_t payload;
  size_t body, frame;
  if (used) *used = 0;
  if (!mulib_sshtransport_ready (s) || !out || !used) return mulib_sshtransport_fail_ (s);
  int status = mulib_sshpacket_peek (p, n, 1, &body, &frame);
  if (status < 0) return mulib_sshtransport_fail_ (s);
  if (!status) return 0;
  if (cap < body) return -2;
  if (!scratch) return mulib_sshtransport_fail_ (s);
  memcpy (scratch, p + 4, body);
  if (!mulib_ssh_gcm_open (&s->rx, p, scratch, body, p + 4 + body)
      || !mulib_sshpacket_payload (scratch, body, &payload)
      || payload.data[0] == MULIB_SSH_KEXINIT || payload.data[0] == MULIB_SSH_NEWKEYS) {
    mulib_secret_wipe (scratch, body);
    return mulib_sshtransport_fail_ (s);
  }
  *out = payload;
  *used = frame;
  return 1;
}
#endif
