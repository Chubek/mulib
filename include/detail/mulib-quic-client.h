/* Implementation of the bounded native QUIC v1 client. Include via mulib-quic.h. */
#define MULIB_QUIC_CLIENT_IMPLEMENTED
#if !defined(MULIB_IMPL_QUIC) || !defined(MULIB_IMPL_TLS)
#error "MULIB_IMPL_QUIC_CLIENT needs MULIB_IMPL_QUIC and the native TLS implementation in this TU"
#endif
#include "../mulib-tls.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

enum
{
  MULIB_QUIC_INITIAL_, MULIB_QUIC_HANDSHAKE_, MULIB_QUIC_APP_,
  MULIB_QUIC_CID_ = 8, MULIB_QUIC_SEND_ = 1200, MULIB_QUIC_MTU_ = 1452,
  MULIB_QUIC_CHUNK_ = 1100, MULIB_QUIC_WINDOW_ = 256,
  MULIB_QUIC_CRYPTO_ = 262144, MULIB_QUIC_UNI_ = 65536,
  MULIB_QUIC_DEFAULT_ = 1048576, MULIB_QUIC_MAX_STREAM_ = 16777216,
  MULIB_QUIC_RETRIES_ = 10, MULIB_QUIC_KEY_LIMIT_ = 1 << 23
};

typedef struct mulib_quic_keys_
{
  mulib_aes_gcm_t aead, hp;
  unsigned char iv[12];
} mulib_quic_keys_;

typedef struct mulib_quic_rx_
{
  unsigned char *data, *present;
  size_t capacity, high, contiguous, final, parsed;
  int fin;
} mulib_quic_rx_;

typedef struct mulib_quic_space_
{
  mulib_quic_keys_ rx, tx;
  mulib_quic_rx_ crypto;
  uint64_t next, largest;
  unsigned char seen[MULIB_QUIC_WINDOW_];
  unsigned have, ack, opened;
} mulib_quic_space_;

typedef struct mulib_quic_tx_
{
  uint64_t limit;
  size_t offset;
  int opened, fin;
} mulib_quic_tx_;

struct mulib_quic_client
{
  int fd, error, stage, confirmed, have_peer;
  uint64_t deadline, tx_max, tx_total, bidi, uni, idle_ms, last_rx;
  size_t max_stream, retry_length, peer_length, retry_cid_length;
  unsigned char original[MULIB_QUIC_CID_], local[MULIB_QUIC_CID_], peer[20], retry_cid[20];
  unsigned char retry[MULIB_QUIC_MTU_], reset_token[16];
  int has_reset_token;
  int path_pending;
  unsigned char path[8];
  char alpn[33];
  mulib_quic_space_ space[3];
  mulib_quic_rx_ rx[4];
  mulib_quic_tx_ tx[4];
  mulib_tls_session_ tls;
  mulib_public_key_t leaf;
  unsigned char handshake[32], server_hs[32], client_hs[32], finished[36];
  uint64_t pending[MULIB_QUIC_RETRIES_];
  size_t pending_count;
  int acknowledged;
};

static int
mulib_quic_fail_ (mulib_quic_client_t *c, int error)
{
  c->error = error;
  return error;
}

static uint64_t
mulib_quic_now_ (void)
{
  struct timespec ts;
  if (clock_gettime (CLOCK_MONOTONIC, &ts))
    return UINT64_MAX;
  return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static uint64_t
mulib_quic_deadline_ (const mulib_quic_client_t *c)
{
  return c->idle_ms && c->last_rx < c->deadline && c->idle_ms < c->deadline - c->last_rx
         ? c->last_rx + c->idle_ms : c->deadline;
}

static int
mulib_quic_wait_ (const mulib_quic_client_t *c, short event, uint64_t until)
{
  struct pollfd p = { c->fd, event, 0 };
  uint64_t deadline = mulib_quic_deadline_ (c), now, left;
  int rc;
  if (until < deadline)
    deadline = until;
  for (;;)
    {
      now = mulib_quic_now_ ();
      if (now >= deadline)
        return MULIB_QUIC_TIMEOUT;
      left = deadline - now;
      rc = poll (&p, 1, left > INT_MAX ? INT_MAX : (int)left);
      if (rc >= 0)
        return rc ? (p.revents & event ? MULIB_QUIC_OK : MULIB_QUIC_IO)
                  : MULIB_QUIC_TIMEOUT;
      if (errno != EINTR)
        return MULIB_QUIC_IO;
    }
}

static int
mulib_quic_keys_derive_ (mulib_quic_keys_ *k, const unsigned char secret[32])
{
  unsigned char key[16], hp[16];
  int ok = mulib_hkdf_tls13 (secret, "quic key", NULL, 0, key, sizeof key)
           && mulib_hkdf_tls13 (secret, "quic iv", NULL, 0, k->iv, sizeof k->iv)
           && mulib_hkdf_tls13 (secret, "quic hp", NULL, 0, hp, sizeof hp)
           && mulib_aes_gcm_init (&k->aead, key) && mulib_aes_gcm_init (&k->hp, hp);
  mulib_secret_wipe (key, sizeof key);
  mulib_secret_wipe (hp, sizeof hp);
  return ok;
}

static int
mulib_quic_initial_keys_ (mulib_quic_space_ *s, const void *cid, size_t n)
{
  static const unsigned char salt[20] = {
    0x38,0x76,0x2c,0xf7,0xf5,0x59,0x34,0xb3,0x4d,0x17,
    0x9a,0xe6,0xa4,0xc8,0x0c,0xad,0xcc,0xbb,0x7f,0x0a
  };
  unsigned char root[32], tx[32], rx[32];
  int ok = mulib_hkdf_extract (MULIB_SHA256, salt, sizeof salt, cid, n, root, sizeof root)
           && mulib_hkdf_tls13 (root, "client in", NULL, 0, tx, sizeof tx)
           && mulib_hkdf_tls13 (root, "server in", NULL, 0, rx, sizeof rx)
           && mulib_quic_keys_derive_ (&s->tx, tx) && mulib_quic_keys_derive_ (&s->rx, rx);
  mulib_secret_wipe (root, sizeof root);
  mulib_secret_wipe (tx, sizeof tx);
  mulib_secret_wipe (rx, sizeof rx);
  return ok;
}

static void
mulib_quic_nonce_ (const mulib_quic_keys_ *k, uint64_t pn, unsigned char nonce[12])
{
  memcpy (nonce, k->iv, 12);
  for (unsigned i = 0; i < 8; ++i)
    nonce[11 - i] ^= (unsigned char)(pn >> (8 * i));
}

static uint64_t
mulib_quic_packet_number_ (uint64_t expected, uint64_t truncated, unsigned bytes)
{
  uint64_t window = UINT64_C (1) << (bytes * 8), half = window / 2;
  uint64_t pn = (expected & ~(window - 1)) | truncated;
  if (pn + half <= expected && pn < (UINT64_C (1) << 62) - window)
    return pn + window;
  if (pn > expected + half && pn >= window)
    return pn - window;
  return pn;
}

static int
mulib_quic_packet_open_ (mulib_quic_space_ *s, unsigned char *p, size_t n,
                         size_t at, uint64_t *number, size_t *header)
{
  unsigned char mask[16], nonce[12];
  uint64_t pn = 0;
  unsigned bytes;
  if (!s->rx.aead.active || n - at < 20)
    return 0;
  mulib_aes_gcm_block_ (&s->rx.hp, p + at + 4, mask);
  p[0] ^= mask[0] & (p[0] & 128 ? 15 : 31);
  bytes = (p[0] & 3) + 1;
  for (unsigned i = 0; i < bytes; ++i)
    {
      p[at + i] ^= mask[i + 1];
      pn = (pn << 8) | p[at + i];
    }
  pn = mulib_quic_packet_number_ (s->have ? s->largest + 1 : 0, pn, bytes);
  at += bytes;
  mulib_quic_nonce_ (&s->rx, pn, nonce);
  if (!mulib_aes_gcm_open (&s->rx.aead, nonce, p, at, p + at, n - at - 16,
                           p + n - 16, p + at))
    return 0;
  if ((p[0] & (p[0] & 128 ? 0x0c : 0x18)) || pn > MULIB_QUIC_MAX_INTEGER
      || (!(p[0] & 128) && (p[0] & 4)))
    return MULIB_QUIC_PROTOCOL;
  if (++s->opened >= MULIB_QUIC_KEY_LIMIT_)
    return MULIB_QUIC_LIMIT;
  *number = pn;
  *header = at;
  return 1;
}

static int
mulib_quic_emit_ (mulib_quic_client_t *c, unsigned level,
                  const unsigned char *payload, size_t length, uint64_t *number)
{
  mulib_quic_space_ *s = c->space + level;
  unsigned char wire[MULIB_QUIC_SEND_], mask[16], nonce[12];
  size_t at = 0, len_at = 0, pn_at, total;
  uint64_t pn = s->next;
  if (!s->tx.aead.active || pn >= MULIB_QUIC_KEY_LIMIT_)
    return MULIB_QUIC_LIMIT;
  if (level == MULIB_QUIC_APP_)
    {
      wire[at++] = 0x43;
      memcpy (wire + at, c->peer, c->peer_length); at += c->peer_length;
    }
  else
    {
      wire[at++] = level == MULIB_QUIC_INITIAL_ ? 0xc3 : 0xe3;
      wire[at++] = 0; wire[at++] = 0; wire[at++] = 0; wire[at++] = 1;
      wire[at++] = (unsigned char)c->peer_length;
      memcpy (wire + at, c->peer, c->peer_length); at += c->peer_length;
      wire[at++] = MULIB_QUIC_CID_;
      memcpy (wire + at, c->local, MULIB_QUIC_CID_); at += MULIB_QUIC_CID_;
      if (level == MULIB_QUIC_INITIAL_)
        {
          mulib_buffer_t token = { wire, at, sizeof wire };
          /* Keep enough room for the entire ClientHello and header. */
          if (c->retry_length > 256
              || !mulib_quic_varint_write (&token, c->retry_length))
            return MULIB_QUIC_LIMIT;
          at = token.length;
          memcpy (wire + at, c->retry, c->retry_length); at += c->retry_length;
        }
      len_at = at; at += 2;
    }
  pn_at = at;
  for (unsigned i = 0; i < 4; ++i)
    wire[at++] = (unsigned char)(pn >> (24 - 8 * i));
  if (length > sizeof wire - at - 16)
    return MULIB_QUIC_LIMIT;
  memcpy (wire + at, payload, length);
  if (level == MULIB_QUIC_INITIAL_)
    {
      memset (wire + at + length, 0, sizeof wire - at - 16 - length);
      length = sizeof wire - at - 16;
    }
  if (len_at)
    {
      size_t encoded = 4 + length + 16;
      wire[len_at] = (unsigned char)(0x40 | (encoded >> 8));
      wire[len_at + 1] = (unsigned char)encoded;
    }
  mulib_quic_nonce_ (&s->tx, pn, nonce);
  if (!mulib_aes_gcm_seal (&s->tx.aead, nonce, wire, at, wire + at, length,
                           wire + at, wire + at + length))
    return MULIB_QUIC_PROTOCOL;
  mulib_aes_gcm_block_ (&s->tx.hp, wire + pn_at + 4, mask);
  wire[0] ^= mask[0] & (level == MULIB_QUIC_APP_ ? 31 : 15);
  for (unsigned i = 0; i < 4; ++i)
    wire[pn_at + i] ^= mask[i + 1];
  total = at + length + 16;
  ++s->next; /* Never encrypt a new payload with this packet number again. */
  while (mulib_quic_now_ () < mulib_quic_deadline_ (c))
    {
      ssize_t sent = send (c->fd, wire, total, 0);
      if (sent >= 0)
        {
          if ((size_t)sent != total)
            return MULIB_QUIC_IO;
          if (number)
            *number = pn;
          return MULIB_QUIC_OK;
        }
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
          int rc = mulib_quic_wait_ (c, POLLOUT, c->deadline);
          if (rc != 1)
            return rc;
        }
      else if (errno != EINTR)
        return MULIB_QUIC_IO;
    }
  return MULIB_QUIC_TIMEOUT;
}

static void
mulib_quic_rx_free_ (mulib_quic_rx_ *r)
{
  free (r->data);
  free (r->present);
  memset (r, 0, sizeof *r);
}

static int
mulib_quic_rx_put_ (mulib_quic_rx_ *r, uint64_t offset, const unsigned char *data,
                    size_t n, int fin, size_t limit)
{
  size_t end, start, next;
  if (offset > limit || n > limit - (size_t)offset)
    return MULIB_QUIC_LIMIT;
  start = (size_t)offset;
  end = start + n;
  if ((r->fin && (end > r->final || (fin && r->final != end)))
      || (fin && end < r->high))
    return MULIB_QUIC_PROTOCOL;
  if (n && end > r->capacity)
    {
      unsigned char *bytes, *present;
      next = r->capacity ? r->capacity : 256;
      while (next < end)
        next *= 2;
      if (next > limit)
        next = limit;
      bytes = (unsigned char *)realloc (r->data, next);
      if (!bytes)
        return MULIB_QUIC_NOMEM;
      r->data = bytes;
      present = (unsigned char *)realloc (r->present, (next + 7) / 8);
      if (!present)
        return MULIB_QUIC_NOMEM;
      memset (present + (r->capacity + 7) / 8, 0,
              (next + 7) / 8 - (r->capacity + 7) / 8);
      r->present = present;
      r->capacity = next;
    }
  for (size_t i = start; i < end; ++i)
    {
      unsigned bit = 1u << (i & 7);
      if (r->present[i / 8] & bit)
        {
          if (r->data[i] != data[i - start])
            return MULIB_QUIC_PROTOCOL;
        }
      else
        {
          r->data[i] = data[i - start];
          r->present[i / 8] |= (unsigned char)bit;
        }
    }
  while (r->contiguous < r->capacity
         && (r->present[r->contiguous / 8] & (1u << (r->contiguous & 7))))
    ++r->contiguous;
  if (end > r->high)
    r->high = end;
  if (fin)
    {
      r->fin = 1;
      r->final = end;
    }
  return 1;
}

static int
mulib_quic_get_ (const unsigned char *p, size_t n, size_t *at, uint64_t *value)
{
  size_t used;
  if (mulib_quic_varint_read (p + *at, n - *at, value, &used) != 1)
    return 0;
  *at += used;
  return 1;
}

/* Fixed-size internal packet builders never allocate. In particular their
 * source can be a separate stack array immediately following the destination. */
static void
mulib_quic_put_ (mulib_buffer_t *b, uint64_t v)
{
  size_t n = v < 64 ? 1 : v < 16384 ? 2 : v < UINT64_C (1073741824) ? 4 : 8;
  unsigned char *p = b->data + b->length;
  b->length += n;
  for (size_t i = n; i; --i)
    {
      p[i - 1] = (unsigned char)v;
      v >>= 8;
    }
  p[0] |= (unsigned char)(n == 1 ? 0 : n == 2 ? 64 : n == 4 ? 128 : 192);
}

static void
mulib_quic_copy_ (mulib_buffer_t *b, const void *p, size_t n)
{
  if (n)
    memcpy (b->data + b->length, p, n);
  b->length += n;
}

static int
mulib_quic_transport_params_ (mulib_quic_client_t *c, const unsigned char *p, size_t n)
{
  uint64_t ids[128], id, len, value;
  size_t at = 0, used, count = 0;
  unsigned mandatory = 0;
  while (at < n)
    {
      if (count == sizeof ids / sizeof *ids)
        return MULIB_QUIC_LIMIT;
      if (!mulib_quic_get_ (p, n, &at, &id) || !mulib_quic_get_ (p, n, &at, &len)
          || len > n - at)
        return MULIB_QUIC_PROTOCOL;
      for (size_t i = 0; i < count; ++i)
        if (ids[i] == id)
          return MULIB_QUIC_PROTOCOL;
      ids[count++] = id;
      if (id == 0 || id == 0x0f || id == 0x10)
        {
          const unsigned char *expected = id == 0 ? c->original : id == 0x0f ? c->peer : c->retry_cid;
          size_t size = id == 0 ? sizeof c->original : id == 0x0f ? c->peer_length : c->retry_cid_length;
          if ((id == 0x10 && !c->retry_length) || len != size || memcmp (p + at, expected, size))
            return MULIB_QUIC_PROTOCOL;
          mandatory |= id == 0 ? 1u : id == 0x0f ? 2u : 4u;
        }
      else if (id == 2)
        {
          if (len != 16)
            return MULIB_QUIC_PROTOCOL;
          memcpy (c->reset_token, p + at, 16);
          c->has_reset_token = 1;
        }
      else if (id == 0x0c)
        {
          if (len)
            return MULIB_QUIC_PROTOCOL;
        }
      else if (id == 0x0d)
        {
          /* The connection stays on its original path. Validate the preferred
           * address envelope, but do not migrate or consume its CID. */
          if (len < 42 || p[at + 24] < 1 || p[at + 24] > 20 || len != 41u + p[at + 24])
            return MULIB_QUIC_PROTOCOL;
        }
      else if (id == 1 || (id >= 3 && id <= 0x0b) || id == 0x0e)
        {
          if (mulib_quic_varint_read (p + at, (size_t)len, &value, &used) != 1 || used != len)
            return MULIB_QUIC_PROTOCOL;
          if ((id == 3 && (value < 1200 || value > 65527))
              || ((id == 8 || id == 9) && value > (UINT64_C (1) << 60))
              || (id == 0x0a && value > 20) || (id == 0x0b && value >= 16384)
              || (id == 0x0e && value < 2))
            return MULIB_QUIC_PROTOCOL;
          switch (id)
            {
            case 1: c->idle_ms = value; break;
            case 4: c->tx_max = value; break;
            case 6: c->tx[0].limit = value; break;
            case 7: for (unsigned i = 1; i < 4; ++i) c->tx[i].limit = value; break;
            case 8: c->bidi = value; break;
            case 9: c->uni = value; break;
            default: break;
            }
        }
      at += (size_t)len;
    }
  return mandatory == (c->retry_length ? 7u : 3u) ? 1 : MULIB_QUIC_PROTOCOL;
}

static int
mulib_quic_tls_extensions_ (mulib_quic_client_t *c, const unsigned char *p, size_t n)
{
  mulib_tls_cursor_ input = { p, n }, exts, v, names, name;
  const unsigned char *type;
  unsigned seen = 0;
  if (!mulib_tls_vec_ (&input, 2, &exts) || input.n)
    return MULIB_QUIC_AUTH;
  while (exts.n)
    {
      unsigned bit;
      if (!mulib_tls_take_ (&exts, 2, &type) || !mulib_tls_vec_ (&exts, 2, &v))
        return MULIB_QUIC_AUTH;
      switch (mulib_tls_u16_ (type))
        {
        case 0:
          bit = 1;
          if (!c->tls.sni || v.n)
            return MULIB_QUIC_AUTH;
          break;
        case 16:
          bit = 2;
          if (!mulib_tls_vec_ (&v, 2, &names) || v.n || !mulib_tls_vec_ (&names, 1, &name)
              || names.n || name.n != strlen (c->alpn) || memcmp (name.p, c->alpn, name.n))
            return MULIB_QUIC_AUTH;
          break;
        case 10:
          bit = 4;
          if (!mulib_tls_vec_ (&v, 2, &names) || v.n || !names.n || (names.n & 1))
            return MULIB_QUIC_AUTH;
          break;
        case 57:
          bit = 8;
          {
            int rc = mulib_quic_transport_params_ (c, v.p, v.n);
            if (rc != 1)
              return rc;
          }
          break;
        default:
          return MULIB_QUIC_AUTH;
        }
      if (seen & bit)
        return MULIB_QUIC_AUTH;
      seen |= bit;
    }
  return (seen & 10) == 10 ? 1 : MULIB_QUIC_AUTH;
}

static int
mulib_quic_tls_key_schedule_ (mulib_quic_client_t *c, const unsigned char *p, size_t n,
                             int application)
{
  struct {
    unsigned char zero[32], empty[32], early[32], derived[32], shared[32], hash[32], master[32], rx[32], tx[32];
  } k = { { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 } };
  int rc = 0;
  (void)mulib_sha (MULIB_SHA256, NULL, 0, k.empty, 32);
  if (!application)
    {
      if (!mulib_tls_serverhello_ (&c->tls, p, n, k.shared, 0))
        goto done;
      (void)mulib_sha_update (&c->tls.transcript, p, n);
      mulib_tls_hash_ (&c->tls, k.hash);
      rc = mulib_hkdf_extract (MULIB_SHA256, k.zero, 32, k.zero, 32, k.early, 32)
           && mulib_hkdf_tls13 (k.early, "derived", k.empty, 32, k.derived, 32)
           && mulib_hkdf_extract (MULIB_SHA256, k.derived, 32, k.shared, 32, c->handshake, 32)
           && mulib_hkdf_tls13 (c->handshake, "s hs traffic", k.hash, 32, c->server_hs, 32)
           && mulib_hkdf_tls13 (c->handshake, "c hs traffic", k.hash, 32, c->client_hs, 32)
           && mulib_quic_keys_derive_ (&c->space[MULIB_QUIC_HANDSHAKE_].rx, c->server_hs)
           && mulib_quic_keys_derive_ (&c->space[MULIB_QUIC_HANDSHAKE_].tx, c->client_hs);
      mulib_secret_wipe (c->tls.private_key, sizeof c->tls.private_key);
    }
  else
    {
      if (n != 36 || !mulib_tls_finished_ (&c->tls, c->server_hs, k.hash)
          || !mulib_secret_equal (k.hash, p + 4, 32))
        goto done;
      (void)mulib_sha_update (&c->tls.transcript, p, n);
      mulib_tls_hash_ (&c->tls, k.hash);
      c->finished[0] = 20; c->finished[3] = 32;
      rc = mulib_hkdf_tls13 (c->handshake, "derived", k.empty, 32, k.derived, 32)
           && mulib_hkdf_extract (MULIB_SHA256, k.derived, 32, k.zero, 32, k.master, 32)
           && mulib_hkdf_tls13 (k.master, "s ap traffic", k.hash, 32, k.rx, 32)
           && mulib_hkdf_tls13 (k.master, "c ap traffic", k.hash, 32, k.tx, 32)
           && mulib_quic_keys_derive_ (&c->space[MULIB_QUIC_APP_].rx, k.rx)
           && mulib_quic_keys_derive_ (&c->space[MULIB_QUIC_APP_].tx, k.tx)
           && mulib_tls_finished_ (&c->tls, c->client_hs, c->finished + 4);
      mulib_secret_wipe (c->handshake, sizeof c->handshake);
      mulib_secret_wipe (c->server_hs, sizeof c->server_hs);
      mulib_secret_wipe (c->client_hs, sizeof c->client_hs);
      mulib_secret_wipe (&c->tls.transcript, sizeof c->tls.transcript);
    }
done:
  mulib_secret_wipe (&k, sizeof k);
  return rc ? 1 : MULIB_QUIC_AUTH;
}

static int
mulib_quic_crypto_ (mulib_quic_client_t *c, unsigned level, uint64_t offset,
                    const unsigned char *p, size_t n)
{
  static const unsigned char types[] = { 2, 8, 11, 15, 20 };
  mulib_quic_rx_ *r = &c->space[level].crypto;
  int rc = mulib_quic_rx_put_ (r, offset, p, n, 0, MULIB_QUIC_CRYPTO_);
  if (rc != 1)
    return rc;
  while (r->contiguous - r->parsed >= 4)
    {
      const unsigned char *msg = r->data + r->parsed;
      size_t size = 4 + mulib_tls_u24_ (msg + 1);
      if (size > MULIB_QUIC_CRYPTO_ - r->parsed)
        return MULIB_QUIC_LIMIT;
      if (size > r->contiguous - r->parsed)
        break;
      if (level == MULIB_QUIC_APP_)
        {
          if (msg[0] != 4 || !mulib_tls_ticket_ (msg + 4, size - 4))
            return MULIB_QUIC_AUTH;
        }
      else
        {
          if (c->stage >= 5 || level != (c->stage ? MULIB_QUIC_HANDSHAKE_ : MULIB_QUIC_INITIAL_)
              || msg[0] != types[c->stage])
            return MULIB_QUIC_AUTH;
          switch (c->stage)
            {
            case 0: rc = mulib_quic_tls_key_schedule_ (c, msg, size, 0); break;
            case 1: rc = mulib_quic_tls_extensions_ (c, msg + 4, size - 4); break;
            case 2:
              rc = mulib_tls_certificates_ (&c->tls, msg, size, &c->leaf) ? 1 : MULIB_QUIC_AUTH;
              break;
            case 3:
              rc = mulib_tls_certverify_ (&c->tls, msg, size, &c->leaf) ? 1 : MULIB_QUIC_AUTH;
              break;
            case 4: rc = mulib_quic_tls_key_schedule_ (c, msg, size, 1); break;
            }
          if (rc != 1)
            return rc;
          if (c->stage > 0 && c->stage < 4)
            (void)mulib_sha_update (&c->tls.transcript, msg, size);
          ++c->stage;
        }
      r->parsed += size;
    }
  return 1;
}

static int
mulib_quic_incoming_ (uint64_t id)
{
  return id == 0 ? 0 : (id <= 11 && id % 4 == 3) ? (int)(id / 4 + 1) : -1;
}

static int
mulib_quic_outgoing_ (uint64_t id)
{
  return id == 0 ? 0 : (id <= 10 && id % 4 == 2) ? (int)(id / 4 + 1) : -1;
}

static int
mulib_quic_frames_ (mulib_quic_client_t *c, unsigned level, const unsigned char *p,
                     size_t n, int *elicits_ack)
{
  size_t at = 0, used;
  uint64_t type, a, b, d;
  int rc, index;
  while (at < n)
    {
      /* Every currently implemented frame type has a one-byte encoding, and
       * QUIC requires frame types (unlike other varints) to use minimum width. */
      type = p[at++];
      if (type > 0x1e || (level != MULIB_QUIC_APP_ && type != 0 && type != 1
                          && type != 2 && type != 3 && type != 6 && type != 0x1c))
        return MULIB_QUIC_PROTOCOL;
      if (type != 0 && type != 2 && type != 3 && type != 0x1c && type != 0x1d)
        *elicits_ack = 1;
      switch (type)
        {
        case 0: case 1:
          break;
        case 2: case 3:
          {
            mulib_quic_ack_t ack;
            mulib_quic_ack_range_t ranges[256];
            rc = mulib_quic_ack_read (p + at - 1, n - at + 1, &ack, ranges,
                                      sizeof ranges / sizeof *ranges, &used);
            if (rc == -2)
              return MULIB_QUIC_LIMIT;
            if (rc != 1 || ranges[0].largest >= c->space[level].next)
              return MULIB_QUIC_PROTOCOL;
            if (level == MULIB_QUIC_APP_)
              for (size_t i = 0; i < ack.range_count && !c->acknowledged; ++i)
                for (size_t j = 0; j < c->pending_count; ++j)
                  if (c->pending[j] >= ranges[i].smallest && c->pending[j] <= ranges[i].largest)
                    {
                      c->acknowledged = 1;
                      break;
                    }
            at += used - 1;
          }
          break;
        case 4: case 5:
          /* There is no partial-failure stream API in this profile. */
          return MULIB_QUIC_PROTOCOL;
        case 6:
          if (!mulib_quic_get_ (p, n, &at, &a) || !mulib_quic_get_ (p, n, &at, &b) || b > n - at)
            return MULIB_QUIC_PROTOCOL;
          rc = mulib_quic_crypto_ (c, level, a, p + at, (size_t)b);
          if (rc != 1)
            return rc;
          at += (size_t)b;
          break;
        case 7:
          if (!mulib_quic_get_ (p, n, &at, &a) || !a || a > n - at)
            return MULIB_QUIC_PROTOCOL;
          at += (size_t)a; /* No resumption or address token storage. */
          break;
        case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15:
          {
            mulib_quic_stream_t stream;
            if (mulib_quic_packet_stream_read (p + at - 1, n - at + 1, &stream, &used) != 1)
              return MULIB_QUIC_PROTOCOL;
            index = mulib_quic_incoming_ (stream.stream_id);
            if (index < 0 || (!index && !c->tx[0].opened))
              return MULIB_QUIC_PROTOCOL;
            rc = mulib_quic_rx_put_ (c->rx + index, stream.offset, stream.data, stream.length,
                                     stream.fin, index ? MULIB_QUIC_UNI_ : c->max_stream);
            if (rc != 1)
              return rc;
            at += used - 1;
          }
          break;
        case 0x10:
          if (!mulib_quic_get_ (p, n, &at, &a))
            return MULIB_QUIC_PROTOCOL;
          if (c->tx_max < a)
            c->tx_max = a;
          break;
        case 0x11:
          if (!mulib_quic_get_ (p, n, &at, &a) || !mulib_quic_get_ (p, n, &at, &b))
            return MULIB_QUIC_PROTOCOL;
          index = mulib_quic_outgoing_ (a);
          if (index < 0 || !c->tx[index].opened)
            return MULIB_QUIC_PROTOCOL;
          if (c->tx[index].limit < b)
            c->tx[index].limit = b;
          break;
        case 0x12: case 0x13: case 0x16: case 0x17:
          if (!mulib_quic_get_ (p, n, &at, &a) || a > (UINT64_C (1) << 60))
            return MULIB_QUIC_PROTOCOL;
          if (type == 0x12 && c->bidi < a)
            c->bidi = a;
          if (type == 0x13 && c->uni < a)
            c->uni = a;
          break;
        case 0x14: case 0x15:
          if (!mulib_quic_get_ (p, n, &at, &a)
              || (type == 0x15 && !mulib_quic_get_ (p, n, &at, &b)))
            return MULIB_QUIC_PROTOCOL;
          break; /* Fixed advertised receive window; no additional credit. */
        case 0x18:
          if (!mulib_quic_get_ (p, n, &at, &a) || !mulib_quic_get_ (p, n, &at, &b)
              || b > a || at == n)
            return MULIB_QUIC_PROTOCOL;
          d = p[at++];
          if (!d || d > 20 || d + 16 > n - at || !c->peer_length)
            return MULIB_QUIC_PROTOCOL;
          /* Spare IDs need not be used on this path. A request to retire the
           * active CID is outside the fixed-CID profile, fail rather than reuse. */
          if (b)
            return MULIB_QUIC_PROTOCOL;
          at += (size_t)d + 16;
          break;
        case 0x19:
          return MULIB_QUIC_PROTOCOL; /* We issued no spare CID. */
        case 0x1a: case 0x1b:
          if (n - at < 8)
            return MULIB_QUIC_PROTOCOL;
          if (type == 0x1a)
            {
              memcpy (c->path, p + at, 8);
              c->path_pending = 1;
            }
          at += 8;
          break;
        case 0x1c: case 0x1d:
          if (!mulib_quic_get_ (p, n, &at, &a)
              || (type == 0x1c && !mulib_quic_get_ (p, n, &at, &b))
              || !mulib_quic_get_ (p, n, &at, &d) || d > n - at)
            return MULIB_QUIC_PROTOCOL;
          if (type == 0x1c && a >= 0x100 && a <= 0x1ff)
            return MULIB_QUIC_AUTH;
          return MULIB_QUIC_CLOSED;
        case 0x1e:
          if (c->stage != 6)
            return MULIB_QUIC_PROTOCOL;
          c->confirmed = 1;
          break;
        }
    }
  return n ? 1 : MULIB_QUIC_PROTOCOL;
}

static int
mulib_quic_retry_ (mulib_quic_client_t *c, const unsigned char *p, size_t n,
                   size_t at, const unsigned char *cid, size_t cid_length)
{
  static const unsigned char key[16] = {
    0xbe,0x0c,0x69,0x0b,0x9f,0x66,0x57,0x5a,0x1d,0x76,0x6b,0x54,0xe3,0x68,0xc8,0x4e
  };
  static const unsigned char nonce[12] = {
    0x46,0x15,0x99,0xd3,0x5d,0x63,0x2b,0xf2,0x23,0x98,0x25,0xbb
  };
  mulib_aes_gcm_t aead;
  unsigned char aad[1 + MULIB_QUIC_CID_ + MULIB_QUIC_MTU_];
  int ok;
  size_t token;
  if (c->have_peer || c->retry_length || !cid_length || n - at <= 16
      || (cid_length == sizeof c->original && !memcmp (cid, c->original, cid_length)))
    return 1;
  aad[0] = sizeof c->original;
  memcpy (aad + 1, c->original, sizeof c->original);
  memcpy (aad + 1 + sizeof c->original, p, n - 16);
  (void)mulib_aes_gcm_init (&aead, key);
  ok = mulib_aes_gcm_open (&aead, nonce, aad, 1 + sizeof c->original + n - 16,
                           NULL, 0, p + n - 16, NULL);
  mulib_aes_gcm_clear (&aead);
  if (!ok)
    return 1;
  token = n - at - 16;
  if (token > 256)
    return MULIB_QUIC_LIMIT;
  memcpy (c->retry, p + at, token); c->retry_length = token;
  memcpy (c->retry_cid, cid, cid_length); c->retry_cid_length = cid_length;
  memcpy (c->peer, cid, cid_length); c->peer_length = cid_length;
  return mulib_quic_initial_keys_ (&c->space[MULIB_QUIC_INITIAL_], cid, cid_length)
         ? 1 : MULIB_QUIC_AUTH;
}

static int
mulib_quic_datagram_ (mulib_quic_client_t *c, const unsigned char *wire, size_t length)
{
  unsigned char p[MULIB_QUIC_MTU_];
  size_t offset = 0;
  while (offset < length)
    {
      const unsigned char *q = wire + offset, *peer_cid = NULL;
      size_t at, n = length - offset, peer_len = 0;
      uint64_t pn, encoded, token = 0;
      unsigned level = MULIB_QUIC_APP_;
      mulib_quic_space_ *s;
      int rc, ack = 0;
      if (!(q[0] & 0x40))
        return 1; /* Fixed bit; version negotiation is not attempted. */
      if (q[0] & 128)
        {
          if (n < 7 || memcmp (q + 1, "\x00\x00\x00\x01", 4) || q[5] != sizeof c->local
              || n < 7 + sizeof c->local || memcmp (q + 6, c->local, sizeof c->local))
            return 1;
          at = 6 + sizeof c->local;
          peer_len = q[at++];
          if (peer_len > 20 || peer_len > n - at)
            return 1;
          peer_cid = q + at; at += peer_len;
          switch ((q[0] >> 4) & 3)
            {
            case 0:
              level = MULIB_QUIC_INITIAL_;
              if (!mulib_quic_get_ (q, n, &at, &token) || token > n - at)
                return 1;
              at += (size_t)token;
              break;
            case 1: /* 0-RTT never comes from a server. */
              return 1;
            case 2:
              level = MULIB_QUIC_HANDSHAKE_;
              break;
            case 3:
              return mulib_quic_retry_ (c, q, n, at, peer_cid, peer_len);
            }
          if (!mulib_quic_get_ (q, n, &at, &encoded) || encoded > n - at)
            return 1;
          n = at + (size_t)encoded;
        }
      else
        {
          at = 1 + sizeof c->local;
          if (n < at || memcmp (q + 1, c->local, sizeof c->local))
            {
              if (c->stage >= 5 && c->has_reset_token && n >= 21
                  && mulib_secret_equal (c->reset_token, q + n - 16, 16))
                return MULIB_QUIC_CLOSED;
              return 1;
            }
        }
      offset += n;
      s = c->space + level;
      memcpy (p, q, n);
      rc = mulib_quic_packet_open_ (s, p, n, at, &pn, &at);
      if (rc < 0)
        return rc;
      if (!rc)
        {
          if (level == MULIB_QUIC_APP_ && c->stage >= 5 && c->has_reset_token
              && n >= 21 && mulib_secret_equal (c->reset_token, q + n - 16, 16))
            return MULIB_QUIC_CLOSED;
          continue;
        }
      if (token)
        return MULIB_QUIC_PROTOCOL;
      if (peer_cid)
        {
          if (c->have_peer)
            {
              if (peer_len != c->peer_length || memcmp (peer_cid, c->peer, peer_len))
                continue;
            }
          else
            {
              memcpy (c->peer, peer_cid, peer_len);
              c->peer_length = peer_len;
              c->have_peer = 1;
            }
        }
      if (s->have && pn <= s->largest
          && (s->largest - pn >= MULIB_QUIC_WINDOW_ || s->seen[pn % MULIB_QUIC_WINDOW_]))
        continue;
      rc = mulib_quic_frames_ (c, level, p + at, n - at - 16, &ack);
      if (rc != 1)
        return rc;
      if (!s->have || pn > s->largest)
        {
          if (!s->have || pn - s->largest >= MULIB_QUIC_WINDOW_)
            memset (s->seen, 0, sizeof s->seen);
          else
            for (uint64_t i = s->largest + 1; i <= pn; ++i)
              s->seen[i % MULIB_QUIC_WINDOW_] = 0;
          s->largest = pn;
          s->have = 1;
        }
      s->seen[pn % MULIB_QUIC_WINDOW_] = 1;
      if (ack)
        s->ack = 1;
      {
        uint64_t now = mulib_quic_now_ ();
        c->last_rx = now < c->deadline ? now : c->deadline;
      }
    }
  return 1;
}

static void
mulib_quic_space_retire_ (mulib_quic_space_ *s)
{
  mulib_quic_rx_free_ (&s->crypto);
  mulib_secret_wipe (s, sizeof *s);
}

static int
mulib_quic_ack_flush_ (mulib_quic_client_t *c)
{
  for (unsigned level = 0; level < 3; ++level)
    {
      mulib_quic_space_ *s = c->space + level;
      unsigned char payload[640];
      mulib_buffer_t frame = { payload, 0, sizeof payload };
      if (s->ack)
        {
          /* Each range and gap spans at most 256 numbers, so fits in two
           * bytes. The 256-position window contains at most 128 ranges. */
          size_t k = 0;
          mulib_quic_ack_range_t ranges[MULIB_QUIC_WINDOW_ / 2];
          uint64_t pn = s->largest;
          for (;;)
            {
              if (s->seen[pn % MULIB_QUIC_WINDOW_])
                {
                  uint64_t top = pn;
                  while (pn && s->largest - (pn - 1) < MULIB_QUIC_WINDOW_
                         && s->seen[(pn - 1) % MULIB_QUIC_WINDOW_])
                    --pn;
                  ranges[k++] = (mulib_quic_ack_range_t){ pn, top };
                }
              if (!pn || s->largest - pn == MULIB_QUIC_WINDOW_ - 1)
                break;
              --pn;
            }
          mulib_quic_put_ (&frame, 2);
          mulib_quic_put_ (&frame, s->largest);
          mulib_quic_put_ (&frame, 0); /* No intentional ACK delay. */
          mulib_quic_put_ (&frame, k - 1);
          mulib_quic_put_ (&frame, ranges[0].largest - ranges[0].smallest);
          for (size_t i = 1; i < k; ++i)
            {
              mulib_quic_put_ (&frame, ranges[i - 1].smallest - ranges[i].largest - 2);
              mulib_quic_put_ (&frame, ranges[i].largest - ranges[i].smallest);
            }
        }
      if (level == MULIB_QUIC_APP_ && c->path_pending)
        {
          mulib_quic_put_ (&frame, 0x1b);
          mulib_quic_copy_ (&frame, c->path, sizeof c->path);
        }
      if (frame.length)
        {
          int rc = mulib_quic_emit_ (c, level, payload, frame.length, NULL);
          if (rc != 1)
            return rc;
          s->ack = 0;
          if (level == MULIB_QUIC_HANDSHAKE_ && c->space[MULIB_QUIC_INITIAL_].tx.aead.active)
            mulib_quic_space_retire_ (&c->space[MULIB_QUIC_INITIAL_]);
          if (level == MULIB_QUIC_APP_)
            c->path_pending = 0;
        }
    }
  return 1;
}

static int
mulib_quic_receive_ (mulib_quic_client_t *c, uint64_t until)
{
  unsigned char wire[MULIB_QUIC_MTU_ + 1];
  for (;;)
    {
      int rc = mulib_quic_wait_ (c, POLLIN, until);
      ssize_t n;
      if (rc != 1)
        return rc;
      n = recv (c->fd, wire, sizeof wire, 0);
      if (n < 0)
        {
          if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
            return MULIB_QUIC_IO;
          continue;
        }
      if ((size_t)n == sizeof wire)
        return 1; /* Larger than the advertised maximum. */
      rc = mulib_quic_datagram_ (c, wire, (size_t)n);
      if (rc != 1)
        return rc;
      /* The client discards these keys on receipt of HANDSHAKE_DONE. */
      if (c->confirmed && c->space[MULIB_QUIC_HANDSHAKE_].tx.aead.active)
        mulib_quic_space_retire_ (&c->space[MULIB_QUIC_HANDSHAKE_]);
      return mulib_quic_ack_flush_ (c);
    }
}

static size_t
mulib_quic_client_params_ (const mulib_quic_client_t *c, unsigned char p[128])
{
  const uint64_t pairs[][2] = {
    { 1, c->deadline - c->last_rx }, { 3, MULIB_QUIC_MTU_ },
    { 4, c->max_stream + 3 * MULIB_QUIC_UNI_ }, { 5, c->max_stream },
    { 7, MULIB_QUIC_UNI_ }, { 9, 3 }, { 0x0b, 0 }, { 0x0e, 8 }
  };
  mulib_buffer_t b = { p, 0, 128 };
  for (size_t i = 0; i < sizeof pairs / sizeof *pairs; ++i)
    {
      unsigned char encoded[8];
      mulib_buffer_t v = { encoded, 0, sizeof encoded };
      mulib_quic_put_ (&v, pairs[i][1]);
      mulib_quic_put_ (&b, pairs[i][0]);
      mulib_quic_put_ (&b, v.length);
      mulib_quic_copy_ (&b, encoded, v.length);
    }
  mulib_quic_put_ (&b, 0x0c); /* disable_active_migration */
  mulib_quic_put_ (&b, 0);
  mulib_quic_put_ (&b, 0x0f);
  mulib_quic_put_ (&b, sizeof c->local);
  mulib_quic_copy_ (&b, c->local, sizeof c->local);
  return b.length;
}

static int
mulib_quic_handshake_send_ (mulib_quic_client_t *c, unsigned level,
                            const unsigned char *bytes, size_t n)
{
  unsigned char p[800];
  mulib_buffer_t b = { p, 0, sizeof p };
  mulib_quic_put_ (&b, 6);
  mulib_quic_put_ (&b, 0);
  mulib_quic_put_ (&b, n);
  mulib_quic_copy_ (&b, bytes, n);
  return mulib_quic_emit_ (c, level, p, b.length, NULL);
}

static uint64_t
mulib_quic_pto_ (unsigned attempt)
{
  /* Conservative application/handshake probe timers. Each retry carries only
   * the single outstanding chunk; no burst above the initial congestion window. */
  return UINT64_C (333) << (attempt < 5 ? attempt : 5);
}

static int
mulib_quic_handshake_ (mulib_quic_client_t *c)
{
  unsigned char params[128], hello[768];
  size_t pn = mulib_quic_client_params_ (c, params);
  size_t n = mulib_tls_make_hello_ (&c->tls, hello, c->alpn, params, pn);
  unsigned attempt = 0;
  uint64_t until = 0;
  int rc;
  if (!n)
    return MULIB_QUIC_AUTH;
  while (!c->confirmed)
    {
      uint64_t now = mulib_quic_now_ ();
      size_t previous_retry = c->retry_length;
      if (now >= mulib_quic_deadline_ (c))
        return MULIB_QUIC_TIMEOUT;
      if (c->stage == 5)
        {
          c->stage = 6;
          attempt = 0;
          until = 0;
        }
      if (now >= until)
        {
          if (attempt == MULIB_QUIC_RETRIES_)
            return MULIB_QUIC_TIMEOUT;
          if (c->stage == 0)
            {
              rc = mulib_quic_handshake_send_ (c, MULIB_QUIC_INITIAL_, hello, n);
              if (rc != 1)
                return rc;
            }
          else if (c->stage == 6)
            {
              rc = mulib_quic_handshake_send_ (c, MULIB_QUIC_HANDSHAKE_, c->finished, sizeof c->finished);
              if (rc != 1)
                return rc;
              if (c->space[MULIB_QUIC_INITIAL_].tx.aead.active)
                mulib_quic_space_retire_ (&c->space[MULIB_QUIC_INITIAL_]);
            }
          until = now + mulib_quic_pto_ (attempt++);
        }
      rc = mulib_quic_receive_ (c, until);
      if (rc < 0)
        return rc;
      if (previous_retry != c->retry_length)
        {
          attempt = 0;
          until = 0;
        }
    }
  mulib_secret_wipe (c->finished, sizeof c->finished);
  return 1;
}

static int
mulib_quic_udp_ (const char *host, unsigned port)
{
  struct addrinfo hints = { 0 }, *list, *p;
  char service[6];
  int fd = -1;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  (void)snprintf (service, sizeof service, "%u", port);
  if (getaddrinfo (host, service, &hints, &list))
    return -1;
  for (p = list; p; p = p->ai_next)
    {
      fd = socket (p->ai_family, p->ai_socktype, p->ai_protocol);
      if (fd < 0)
        continue;
      if (fcntl (fd, F_SETFD, FD_CLOEXEC) == 0 && fcntl (fd, F_SETFL, O_NONBLOCK) == 0
          && connect (fd, p->ai_addr, p->ai_addrlen) == 0)
        break;
      (void)close (fd);
      fd = -1;
    }
  freeaddrinfo (list);
  return fd;
}

static void
mulib_quic_destroy_ (mulib_quic_client_t *c)
{
  if (c->fd >= 0)
    (void)close (c->fd);
  for (unsigned i = 0; i < 3; ++i)
    mulib_quic_rx_free_ (&c->space[i].crypto);
  for (unsigned i = 0; i < 4; ++i)
    mulib_quic_rx_free_ (&c->rx[i]);
  mulib_x509_store_free (&c->tls.owned_trust);
  mulib_secret_wipe (c, sizeof *c);
  free (c);
}

int
mulib_quic_client_connect (const char *host, unsigned port,
                           const mulib_quic_client_options_t *options, mulib_quic_client_t **out)
{
  mulib_quic_client_t *c;
  const char *alpn = options && options->alpn ? options->alpn : "h3";
  size_t max = options && options->max_stream_bytes ? options->max_stream_bytes : MULIB_QUIC_DEFAULT_;
  uint64_t timeout = options && options->timeout_ms ? options->timeout_ms : 10000;
  int rc = MULIB_QUIC_AUTH;
  if (!out || *out || !host || !*host || strlen (host) > 253 || !*alpn
      || strlen (alpn) > 32 || !port || port > 65535 || max > MULIB_QUIC_MAX_STREAM_)
    return MULIB_QUIC_INVALID;
  c = (mulib_quic_client_t *)calloc (1, sizeof *c);
  if (!c)
    return MULIB_QUIC_NOMEM;
  c->fd = -1;
  c->max_stream = max;
  strcpy (c->alpn, alpn);
  strcpy (c->tls.host, host);
  if (options && options->trust)
    c->tls.trust = options->trust;
  else
    {
      if (!(options && options->ca_file ? mulib_x509_store_file (&c->tls.owned_trust, options->ca_file)
                                        : mulib_x509_store_system (&c->tls.owned_trust)))
        goto fail;
      c->tls.trust = &c->tls.owned_trust;
    }
  if (!c->tls.trust->count || !mulib_entropy (c->local, sizeof c->local)
      || !mulib_entropy (c->original, sizeof c->original)
      || !mulib_quic_initial_keys_ (&c->space[MULIB_QUIC_INITIAL_], c->original, sizeof c->original))
    goto fail;
  memcpy (c->peer, c->original, sizeof c->original);
  c->peer_length = sizeof c->original;
  c->fd = mulib_quic_udp_ (host, port);
  c->last_rx = mulib_quic_now_ ();
  if (c->fd < 0 || c->last_rx == UINT64_MAX)
    {
      rc = MULIB_QUIC_IO;
      goto fail;
    }
  c->deadline = c->last_rx + timeout;
  rc = mulib_quic_handshake_ (c);
  if (rc != 1)
    goto fail;
  mulib_x509_store_free (&c->tls.owned_trust);
  c->tls.trust = NULL;
  *out = c;
  return 1;
fail:
  mulib_quic_destroy_ (c);
  return rc;
}

int
mulib_quic_client_send (mulib_quic_client_t *c, uint64_t id, const void *data, size_t n, int fin)
{
  const unsigned char *p = (const unsigned char *)data;
  int index = mulib_quic_outgoing_ (id);
  mulib_quic_tx_ *s;
  size_t limit;
  if (!c || index < 0 || (!data && n) || (fin != 0 && fin != 1))
    return MULIB_QUIC_INVALID;
  if (c->error != 0)
    return c->error;
  s = c->tx + index;
  if (s->fin)
    return MULIB_QUIC_INVALID;
  limit = index ? MULIB_QUIC_UNI_ : c->max_stream;
  if (n > limit - s->offset)
    return MULIB_QUIC_LIMIT;
  if (!n && !fin)
    return 1;
  while (n || fin)
    {
      unsigned char payload[MULIB_QUIC_CHUNK_ + 32];
      mulib_buffer_t frame = { payload, 0, sizeof payload };
      size_t len = n < MULIB_QUIC_CHUNK_ ? n : MULIB_QUIC_CHUNK_;
      int last = fin && len == n, rc;
      while ((index ? c->uni < (uint64_t)index : !c->bidi)
             || s->offset + len > s->limit || len > c->tx_max - c->tx_total)
        {
          uint64_t credit = s->limit > s->offset ? s->limit - s->offset : 0;
          if (credit > c->tx_max - c->tx_total)
            credit = c->tx_max - c->tx_total;
          if (credit && len && (index ? c->uni >= (uint64_t)index : c->bidi != 0))
            {
              len = (size_t)credit; /* Credit is less than the proposed chunk. */
              last = 0;
              break;
            }
          rc = mulib_quic_receive_ (c, c->deadline);
          if (rc != 1)
            {
              mulib_quic_fail_ (c, rc ? rc : MULIB_QUIC_CLOSED);
              return rc;
            }
        }
      mulib_quic_put_ (&frame, (uint64_t)(14 | last));
      mulib_quic_put_ (&frame, id);
      mulib_quic_put_ (&frame, s->offset);
      mulib_quic_put_ (&frame, len);
      mulib_quic_copy_ (&frame, p, len);
      if (!index)
        s->opened = 1;
      else
        for (int i = 1; i <= index; ++i)
          c->tx[i].opened = 1;
      c->acknowledged = 0;
      c->pending_count = 0;
      for (unsigned attempt = 0; attempt < MULIB_QUIC_RETRIES_ && !c->acknowledged; ++attempt)
        {
          uint64_t now = mulib_quic_now_ (), until;
          if (now >= mulib_quic_deadline_ (c))
            break;
          until = now + mulib_quic_pto_ (attempt);
          rc = mulib_quic_emit_ (c, MULIB_QUIC_APP_, payload, frame.length, c->pending + c->pending_count);
          if (rc != 1)
            {
              mulib_quic_fail_ (c, rc ? rc : MULIB_QUIC_CLOSED);
              return rc;
            }
          ++c->pending_count;
          while (!c->acknowledged && mulib_quic_now_ () < until)
            {
              rc = mulib_quic_receive_ (c, until);
              if (rc < 0)
                return mulib_quic_fail_ (c, rc);
              if (!rc)
                break;
            }
        }
      c->pending_count = 0;
      if (!c->acknowledged)
        {
          mulib_quic_fail_ (c, MULIB_QUIC_CLOSED);
          return MULIB_QUIC_TIMEOUT;
        }
      s->offset += len; c->tx_total += len; n -= len;
      if (len)
        p += len;
      if (last)
        {
          s->fin = 1;
          break;
        }
    }
  return 1;
}

int
mulib_quic_client_poll (mulib_quic_client_t *c)
{
  int rc;
  if (!c)
    return MULIB_QUIC_INVALID;
  if (c->error)
    return c->error;
  rc = mulib_quic_receive_ (c, c->deadline);
  if (rc != 1)
    mulib_quic_fail_ (c, rc ? rc : MULIB_QUIC_CLOSED);
  return rc;
}

int
mulib_quic_client_stream (const mulib_quic_client_t *c, uint64_t id, mulib_quic_stream_t *out)
{
  int i = mulib_quic_incoming_ (id);
  const mulib_quic_rx_ *r;
  if (!c || !out || i < 0)
    return MULIB_QUIC_INVALID;
  if (c->error)
    return c->error;
  r = c->rx + i;
  *out = (mulib_quic_stream_t){ id, 0, r->contiguous ? r->data : NULL, r->contiguous,
                               r->fin && r->final == r->contiguous };
  return 1;
}

void
mulib_quic_client_close (mulib_quic_client_t *c)
{
  if (c)
    {
      if (c->error != MULIB_QUIC_CLOSED && c->confirmed)
        {
          const unsigned char close_frame[] = { 0x1c, 0, 0, 0 };
          (void)mulib_quic_emit_ (c, MULIB_QUIC_APP_, close_frame, sizeof close_frame, NULL);
        }
      mulib_quic_destroy_ (c);
    }
}
