#include "mulib-sshtransport.h"
#include <assert.h>
#include <string.h>

typedef struct {
  unsigned char data[MULIB_SSH_KEX_OUTPUT_MIN];
  size_t n;
} batch_t;

static const unsigned char seed[32] = {4, 5, 6};
static const unsigned char vc[] = "SSH-2.0-client First comment";
static const unsigned char vs[] = "SSH-2.0-server Another comment";
static const unsigned char app[] = {MULIB_SSH_CHANNEL_DATA, 1, 2, 3};

static int
zero (const void *data, size_t n)
{
  const unsigned char *p = data;
  while (n--)
    if (*p++) return 0;
  return 1;
}

static void
start (mulib_sshtransport_t *c, mulib_sshtransport_t *s, batch_t *co, batch_t *so, int bad_pin)
{
  const mulib_ssh_slice_t client = {vc, sizeof vc - 1}, server = {vs, sizeof vs - 1};
  unsigned char pin[32];
  assert (mulib_ed25519_public (pin, seed));
  pin[0] ^= bad_pin != 0;
  assert (mulib_sshtransport_start (c, MULIB_SSH_CLIENT, client, server, pin, co->data,
                                    sizeof co->data, &co->n));
  assert (mulib_sshtransport_start (s, MULIB_SSH_SERVER, client, server, seed, so->data,
                                    sizeof so->data, &so->n));
  assert (!mulib_sshtransport_session_id (c, pin));
}

static void
consume (batch_t *in, size_t used)
{
  assert (used > 0 && used <= in->n);
  in->n -= used;
  memmove (in->data, in->data + used, in->n);
}

static void
clear_step (mulib_sshtransport_t *s, batch_t *in, batch_t *out)
{
  size_t used, body, frame;
  assert (mulib_sshpacket_peek (in->data, in->n, 0, &body, &frame) == 1);
  for (size_t i = 0; i < frame; ++i) {
    assert (!mulib_sshtransport_handshake (s, in->data, i, &used, out->data,
                                           sizeof out->data, &out->n));
    assert (!used && !out->n);
  }
  assert (mulib_sshtransport_handshake (s, in->data, in->n, &used, out->data,
                                        sizeof out->data - 1, &out->n) == -2);
  assert (!used && !out->n);
  assert (mulib_sshtransport_handshake (s, in->data, in->n, &used, out->data,
                                        sizeof out->data, &out->n) == 1);
  assert (used == frame);
  consume (in, used);
}

static void
ready (mulib_sshtransport_t *c, mulib_sshtransport_t *s)
{
  batch_t co, so, c2, s2;
  start (c, s, &co, &so, 0);
  clear_step (s, &co, &s2);
  assert (!co.n && !s2.n);
  clear_step (c, &so, &c2);
  assert (!so.n && c2.n);
  clear_step (s, &c2, &so);
  assert (!c2.n && so.n);
  assert (s->tx.active && !s->rx.active && s->next_rx.active);
  clear_step (c, &so, &co);
  assert (c->tx.active && !c->rx.active && c->next_rx.active);
  assert (!mulib_sshtransport_ready (c) && !mulib_sshtransport_ready (s));
  clear_step (s, &co, &s2);
  assert (!co.n && !s2.n);
  clear_step (c, &so, &c2);
  assert (!so.n && !c2.n);
  assert (mulib_sshtransport_ready (c) && mulib_sshtransport_ready (s));
  assert (!mulib_sshtransport_rekey_due (c) && !mulib_sshtransport_rekey_due (s));
  assert (!memcmp (c->session_id, s->session_id, sizeof c->session_id));
}

static void
append_app (mulib_sshtransport_t *s, batch_t *out)
{
  size_t n;
  assert (mulib_sshtransport_seal (s, app, sizeof app, out->data + out->n,
                                   sizeof out->data - out->n, &n) == 1);
  out->n += n;
}

static void
step (mulib_sshtransport_t *s, batch_t *in, batch_t *out, int application)
{
  unsigned char scratch[MULIB_SSH_BODY_MAX];
  mulib_ssh_slice_t payload = {app, 999};
  size_t used, body, frame;
  assert (mulib_sshpacket_peek (in->data, in->n, 1, &body, &frame) == 1);
  uint32_t packets = s->rx.packets;
  for (size_t i = 0; i < frame; ++i) {
    assert (!mulib_sshtransport_receive (s, in->data, i, scratch, 0, &payload,
                                         &used, out->data, 0, &out->n));
    assert (!used && !out->n && payload.data == app && payload.length == 999);
  }
  assert (mulib_sshtransport_receive (s, in->data, in->n, scratch, body - 1, &payload,
                                      &used, out->data, sizeof out->data, &out->n) == -2);
  assert (!used && !out->n && s->rx.packets == packets && payload.length == 999);
  assert (mulib_sshtransport_receive (s, in->data, in->n, scratch, sizeof scratch, &payload,
                                      &used, out->data, sizeof out->data - 1, &out->n) == -2);
  assert (!used && !out->n && s->rx.packets == packets && payload.length == 999);
  assert (mulib_sshtransport_receive (s, in->data, in->n, scratch, sizeof scratch, &payload,
                                      &used, out->data, sizeof out->data, &out->n) == 1);
  assert (used == frame);
  consume (in, used);
  if (application) {
    assert (!out->n && payload.data == scratch + 1 && payload.length == sizeof app
            && !memcmp (payload.data, app, sizeof app));
    mulib_secret_wipe (scratch, body);
  } else {
    assert (!payload.data && !payload.length && zero (scratch, body));
  }
}

/* mode: client-, server-, or locally simultaneous initiation. */
static void
rekey (mulib_sshtransport_t *c, mulib_sshtransport_t *s, int mode)
{
  unsigned char sid[32], other[32], expected_iv[12], secret[32], key[32], h[32];
  batch_t co = {{0}, 0}, so = {{0}, 0}, c2, s2;
  assert (mulib_sshtransport_session_id (c, sid));
  size_t written;
  assert (mulib_sshtransport_rekey (c, c2.data, sizeof c2.data - 1, &written) == -2 && !written);
  assert (mulib_sshtransport_ready (c));
  /* Old application records in both directions must precede KEXINIT. */
  append_app (c, &co);
  append_app (s, &so);
  if (mode != 1) {
    assert (mulib_sshtransport_rekey (c, c2.data, sizeof c2.data, &c2.n) == 1);
    assert (mulib_sshtransport_rekey (c, s2.data, sizeof s2.data, &s2.n) == 0 && !s2.n);
    assert (mulib_sshtransport_seal (c, app, sizeof app, s2.data, sizeof s2.data, &s2.n) == -3);
    assert (!s2.n && !mulib_sshtransport_ready (c));
    assert (mulib_sshtransport_session_id (c, other) && !memcmp (sid, other, 32));
    memcpy (co.data + co.n, c2.data, c2.n);
    co.n += c2.n;
  }
  if (mode != 0) {
    assert (mulib_sshtransport_rekey (s, s2.data, sizeof s2.data, &s2.n) == 1);
    memcpy (so.data + so.n, s2.data, s2.n);
    so.n += s2.n;
  }
  step (c, &so, &c2, 1);
  step (s, &co, &s2, 1);
  if (mode == 0) {
    step (s, &co, &so, 0);
    assert (!co.n && so.n);
    step (c, &so, &co, 0);
    assert (!so.n && co.n);
  } else if (mode == 1) {
    step (c, &so, &co, 0); /* emits its KEXINIT and ECDH_INIT together */
    assert (!so.n && co.n);
    step (s, &co, &s2, 0);
    assert (co.n && !s2.n);
  } else {
    step (c, &so, &c2, 0);
    step (s, &co, &s2, 0);
    assert (!co.n && !so.n && c2.n && !s2.n);
    co = c2;
  }
  step (s, &co, &so, 0); /* emits ECDH_REPLY and old-key NEWKEYS together */
  assert (!co.n && so.n && s->tx.packets == 0 && s->rx.packets > 0);
  assert (s->next_rx.active && zero (s->scalar, sizeof s->scalar));
  assert (mulib_sshtransport_session_id (s, other) && !memcmp (sid, other, 32));
  append_app (s, &so); /* new-key traffic before the peer's NEWKEYS arrives */
  const mulib_ssh_transcript_t t = {
    {vc, sizeof vc - 1}, {vs, sizeof vs - 1},
    {c->local_init, c->local_init_length}, {c->peer_init, c->peer_init_length}
  };
  assert (mulib_x25519 (secret, c->scalar, s->ephemeral));
  assert (mulib_ed25519_public (key, seed));
  assert (mulib_sshkex_hash (&t, key, c->ephemeral, s->ephemeral, secret, h));
  assert (memcmp (h, sid, 32));
  assert (mulib_ssh_derive (secret, h, sid, 'A', expected_iv, sizeof expected_iv));
  step (c, &so, &co, 0);
  assert (c->tx.packets == 0 && c->rx.packets > 0 && c->next_rx.active);
  assert (!memcmp (expected_iv, c->tx.nonce, sizeof expected_iv));
  assert (!memcmp (expected_iv, s->next_rx.nonce, sizeof expected_iv));
  assert (co.n && so.n && !mulib_sshtransport_ready (c));
  append_app (c, &co);
  step (c, &so, &c2, 0);
  assert (mulib_sshtransport_ready (c) && c->rx.packets == 0 && !c->next_rx.active);
  assert (so.n && !c2.n);
  step (c, &so, &c2, 1);
  step (s, &co, &s2, 0);
  assert (mulib_sshtransport_ready (s) && s->rx.packets == 0 && !s->next_rx.active);
  assert (co.n && !s2.n);
  step (s, &co, &s2, 1);
  assert (!co.n && !so.n);
  assert (mulib_sshtransport_session_id (c, other) && !memcmp (sid, other, 32));
  assert (mulib_sshtransport_session_id (s, other) && !memcmp (sid, other, 32));
}

static int
padding (void *unused, void *out, size_t n)
{
  (void)unused;
  memset (out, 42, n);
  return 1;
}

static void
guessed_packet (void)
{
  mulib_sshtransport_t c, s;
  batch_t co, so, c2, s2;
  unsigned char proposal[MULIB_SSH_KEX_OUTPUT_MIN];
  const unsigned char kex_names[] = "diffie-hellman-group14-sha256,curve25519-sha256";
  const unsigned char guessed[] = {MULIB_SSH_ECDH_INIT};
  mulib_ssh_kexinit_t msg;
  size_t n, used;
  start (&c, &s, &co, &so, 0);
  assert (mulib_ssh_kexinit_parse (c.local_init, c.local_init_length, &msg));
  assert (mulib_ssh_slice_is (msg.lists[MULIB_SSH_LIST_KEX], MULIB_SSH_CURVE25519_NAME));
  msg.lists[MULIB_SSH_LIST_KEX] = (mulib_ssh_slice_t){kex_names, sizeof kex_names - 1};
  msg.follows = 1;
  assert (mulib_ssh_kexinit_write (&msg, proposal, sizeof proposal, &n));
  memcpy (c.local_init, proposal, n);
  c.local_init_length = n;
  assert (mulib_sshpacket_write (proposal, n, 0, co.data, sizeof co.data, &co.n, padding, NULL));
  assert (mulib_sshpacket_write (guessed, sizeof guessed, 0, co.data + co.n,
                                  sizeof co.data - co.n, &used, padding, NULL));
  co.n += used;
  clear_step (&s, &co, &s2);
  assert (s.skip_guess && co.n && !s2.n);
  clear_step (&s, &co, &s2);
  assert (!s.skip_guess && !co.n && !s2.n);
  clear_step (&c, &so, &co);
  clear_step (&s, &co, &so);
  clear_step (&c, &so, &co);
  clear_step (&c, &so, &c2);
  clear_step (&s, &co, &s2);
  assert (!co.n && !so.n && !c2.n && !s2.n);
  assert (mulib_sshtransport_ready (&c) && mulib_sshtransport_ready (&s));
  assert (!memcmp (c.session_id, s.session_id, sizeof s.session_id));
  mulib_sshtransport_clear (&c);
  mulib_sshtransport_clear (&s);
}

static void
bad_kex (int mode)
{
  mulib_sshtransport_t c, s;
  batch_t co, so, c2, s2;
  size_t used;
  start (&c, &s, &co, &so, mode == 0);
  clear_step (&s, &co, &s2);
  clear_step (&c, &so, &co);
  if (mode == 2) {
    unsigned char key[32] = {0}, p[MULIB_SSH_ECDH_INIT_SIZE];
    size_t n;
    assert (mulib_sshkex_init_write (key, p, sizeof p, &n));
    assert (mulib_sshpacket_write (p, n, 0, co.data, sizeof co.data, &co.n, padding, NULL));
    assert (mulib_sshtransport_handshake (&s, co.data, co.n, &used, so.data,
                                          sizeof so.data, &so.n) == -1);
    assert (!used && !so.n && zero (&s, sizeof s));
  } else {
    clear_step (&s, &co, &so);
    if (mode == 1) {
      mulib_ssh_slice_t p, host, e, sig;
      size_t body, frame;
      assert (mulib_sshpacket_peek (so.data, so.n, 0, &body, &frame) == 1);
      assert (mulib_sshpacket_payload (so.data + 4, body, &p));
      assert (mulib_sshkex_reply_parse (p.data, p.length, &host, &e, &sig));
      so.data[sig.data - so.data] ^= 1;
    }
    assert (mulib_sshtransport_handshake (&c, so.data, so.n, &used, c2.data,
                                          sizeof c2.data, &c2.n) == -1);
    assert (!used && !c2.n && zero (&c, sizeof c));
  }
  mulib_sshtransport_clear (&c);
  mulib_sshtransport_clear (&s);
}

static void
bad_record (int mode)
{
  mulib_sshtransport_t c, s;
  batch_t f = {{0}, 0}, out;
  unsigned char scratch[MULIB_SSH_BODY_MAX];
  mulib_ssh_slice_t p = {app, 999};
  size_t used, body, frame;
  ready (&c, &s);
  if (mode < 3) append_app (&c, &f);
  else {
    /* Authenticated, but invalid: padding below four, unexpected NEWKEYS,
     * or application traffic before a sender has transmitted NEWKEYS. */
    const unsigned char newkeys = MULIB_SSH_NEWKEYS;
    if (mode == 5) {
      assert (mulib_sshtransport_rekey (&c, f.data, sizeof f.data, &f.n) == 1);
      step (&s, &f, &out, 0);
    }
    const void *data = mode == 4 ? &newkeys : app;
    assert (mulib_sshpacket_write (data, mode == 4 ? 1 : sizeof app, 1, f.data,
                                    sizeof f.data, &f.n, padding, NULL));
    if (mode == 3) f.data[4] = 3;
    assert (mulib_ssh_gcm_seal (&c.tx, f.data, f.data + 4, f.n - 4, f.data + f.n));
    f.n += 16;
  }
  if (mode == 0) f.data[f.n - 1] ^= 1; /* tag */
  if (mode == 1) { /* aligned, in-range length tampering */
    mulib_endian_store_be32 (f.data, 32);
    f.n += 16;
  }
  if (mode == 2) { /* replay: first legacy open succeeds, second fails */
    assert (mulib_sshtransport_open (&s, f.data, f.n, scratch, sizeof scratch, &p, &used) == 1);
    assert (p.length == sizeof app && !memcmp (p.data, app, sizeof app));
    p.data = app;
    p.length = 999;
  }
  assert (mulib_sshpacket_peek (f.data, f.n, 1, &body, &frame) == 1);
  assert (mulib_sshtransport_receive (&s, f.data, f.n, scratch, sizeof scratch, &p, &used,
                                      out.data, sizeof out.data, &out.n) == -1);
  assert (!used && !out.n && p.data == app && p.length == 999);
  assert (zero (scratch, body) && zero (&s, sizeof s));
  mulib_sshtransport_clear (&c);
}

static void
exhaustion (int mode)
{
  mulib_sshtransport_t c, s;
  batch_t f = {{0}, 0};
  size_t used;
  ready (&c, &s);
  assert (mulib_sshtransport_seal (&c, app, sizeof app, f.data, 1, &used) == -2);
  assert (!used && !c.tx.packets);
  if (mode == 0) c.tx.packets = MULIB_SSH_GCM_PACKET_LIMIT;
  if (mode == 1) c.tx.bytes = MULIB_SSH_GCM_BYTE_LIMIT - 15;
  if (mode == 2) mulib_endian_store_be64 (c.tx.nonce + 4, UINT64_MAX);
  if (mode == 3) {
    unsigned char scratch[MULIB_SSH_BODY_MAX];
    mulib_ssh_slice_t p = {app, 999};
    append_app (&c, &f);
    s.rx.bytes = MULIB_SSH_GCM_BYTE_LIMIT;
    assert (mulib_sshtransport_rekey_due (&s));
    assert (mulib_sshtransport_open (&s, f.data, f.n, scratch, sizeof scratch, &p, &used) == -1);
    assert (!used && p.data == app && p.length == 999 && zero (scratch, 16) && zero (&s, sizeof s));
  } else {
    assert (mulib_sshtransport_rekey_due (&c));
    memset (f.data, 0xff, sizeof f.data);
    assert (mulib_sshtransport_seal (&c, app, sizeof app, f.data, sizeof f.data, &used) == -1);
    assert (!used && zero (&c, sizeof c) && zero (f.data, 36));
  }
  mulib_sshtransport_clear (&c);
  mulib_sshtransport_clear (&s);
}

static void
failed_newkeys (int outbound)
{
  mulib_sshtransport_t c, s;
  batch_t co, so, c2;
  unsigned char scratch[MULIB_SSH_BODY_MAX];
  size_t used, body, frame;
  mulib_ssh_slice_t p = {app, 999};
  ready (&c, &s);
  assert (mulib_sshtransport_rekey (&c, co.data, sizeof co.data, &co.n) == 1);
  step (&s, &co, &so, 0);
  step (&c, &so, &co, 0);
  if (outbound) {
    /* Enough old-key budget to produce the reply but not NEWKEYS: neither
     * should be exposed and both sets of traffic keys must be destroyed. */
    s.tx.packets = MULIB_SSH_GCM_PACKET_LIMIT - 1;
    memset (so.data, 0xff, sizeof so.data);
    assert (mulib_sshtransport_receive (&s, co.data, co.n, scratch, sizeof scratch, &p, &used,
                                        so.data, sizeof so.data, &so.n) == -1);
    size_t batch = 40 + mulib_sshpacket_body_size (MULIB_SSH_ECDH_REPLY_SIZE, 1)
                      + mulib_sshpacket_body_size (1, 1);
    assert (!used && !so.n && p.length == 999 && zero (&s, sizeof s) && zero (so.data, batch));
  } else {
    step (&s, &co, &so, 0);
    step (&c, &so, &co, 0);
    assert (c.next_rx.active && mulib_sshpacket_peek (so.data, so.n, 1, &body, &frame) == 1);
    so.data[frame - 1] ^= 1;
    assert (mulib_sshtransport_receive (&c, so.data, so.n, scratch, sizeof scratch, &p, &used,
                                        c2.data, sizeof c2.data, &c2.n) == -1);
    assert (!used && !c2.n && p.length == 999 && zero (scratch, body) && zero (&c, sizeof c));
  }
  mulib_sshtransport_clear (&c);
  mulib_sshtransport_clear (&s);
}

static void
maximum_record (mulib_sshtransport_t *c, mulib_sshtransport_t *s)
{
  unsigned char data[MULIB_SSH_PAYLOAD_MAX + 1], frame[MULIB_SSH_FRAME_MAX];
  unsigned char scratch[MULIB_SSH_BODY_MAX];
  size_t n, used;
  mulib_ssh_slice_t p;
  memset (data, MULIB_SSH_CHANNEL_DATA, sizeof data);
  assert (mulib_sshtransport_seal (c, data, sizeof data - 1, frame, sizeof frame, &n) == 1);
  assert (mulib_sshtransport_open (s, frame, n, scratch, sizeof scratch, &p, &used) == 1);
  assert (n == used && p.length == sizeof data - 1 && !memcmp (p.data, data, p.length));
  assert (mulib_sshtransport_seal (c, data, sizeof data, frame, sizeof frame, &n) == -1);
  assert (!n && zero (c, sizeof *c));
}

int
main (void)
{
  mulib_sshtransport_t c, s;
  ready (&c, &s);
  c.tx.bytes = s.rx.bytes = MULIB_SSH_GCM_BYTE_LIMIT - (UINT64_C (32) << 20);
  assert (mulib_sshtransport_rekey_due (&c) && mulib_sshtransport_rekey_due (&s));
  for (int mode = 0; mode < 3; ++mode) rekey (&c, &s, mode);
  assert (!mulib_sshtransport_rekey_due (&c) && !mulib_sshtransport_rekey_due (&s));
  maximum_record (&c, &s);
  mulib_sshtransport_clear (&c);
  mulib_sshtransport_clear (&s);
  guessed_packet ();
  for (int mode = 0; mode < 3; ++mode) bad_kex (mode);
  for (int mode = 0; mode < 6; ++mode) bad_record (mode);
  for (int mode = 0; mode < 4; ++mode) exhaustion (mode);
  failed_newkeys (0);
  failed_newkeys (1);
  return 0;
}
