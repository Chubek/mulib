#define MULIB_IMPL_XTKS
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_PACKETSTREAM
#define MULIB_IMPL_TUNNEL
#define MULIB_IMPL_ABSTUN
#define MULIB_IMPL_OBS4
#define MULIB_IMPL_SHADOWSOCKS
#define MULIB_IMPL_TSL
#include "mulib-abstun.h"
#include "mulib-obs4.h"
#include "mulib-packetstream.h"
#include "mulib-shadowsocks.h"
#include "mulib-tunnel.h"
#include "mulib-tsl.h"
#include <assert.h>
#include <string.h>

typedef struct memory_io
{
  const unsigned char *input;
  size_t input_size, read_pos;
  unsigned char output[256];
  size_t write_pos;
  int block, bad, closed, handshakes, auth;
} memory_io;
static int
read_memory (void *ctx, void *p, size_t n, size_t *count)
{
  memory_io *m = ctx;
  *count = 0;
  if (m->bad)
    {
      *count = n + 1;
      return 0;
    }
  if (m->block)
    {
      m->block = 0;
      return 1;
    }
  if (m->read_pos == m->input_size)
    return 2;
  if (n > 3)
    n = 3;
  if (n > m->input_size - m->read_pos)
    n = m->input_size - m->read_pos;
  memcpy (p, m->input + m->read_pos, n);
  m->read_pos += n;
  *count = n;
  return 0;
}
static int
write_memory (void *ctx, const void *p, size_t n, size_t *count)
{
  memory_io *m = ctx;
  *count = 0;
  if (m->bad)
    return 0; /* invalid zero-progress success */
  if (m->block)
    {
      m->block = 0;
      return 1;
    }
  if (n > 2)
    n = 2;
  assert (n <= sizeof m->output - m->write_pos);
  memcpy (m->output + m->write_pos, p, n);
  m->write_pos += n;
  *count = n;
  return 0;
}
static void
streams (void)
{
  memory_io m = { 0 };
  mulib_xtks_t s = { &m, read_memory, write_memory };
  size_t off = 0, n;
  char output[16];
  m.input = (const unsigned char *)"abcde";
  m.input_size = 5;
  m.block = 1;
  assert (mulib_xtks_read_exact (&s, output, 5, &off) == 1 && off == 0);
  assert (mulib_xtks_read_exact (&s, output, 5, &off) == 0 && off == 5
          && !memcmp (output, "abcde", 5));
  off = 0;
  assert (mulib_xtks_read_exact (&s, output, 5, &off) == 2 && off == 0);
  m.block = 1;
  assert (mulib_xtks_write_all (&s, "hello", 5, &off) == 1 && off == 0);
  assert (mulib_xtks_write_all (&s, "hello", 5, &off) == 0 && off == 5);
  assert (m.write_pos == 5 && !memcmp (m.output, "hello", 5));
  assert (!mulib_xtks_write (&s, NULL, 0, &n) && n == 0);
  m.bad = 1;
  assert (mulib_xtks_write (&s, "x", 1, &n) == -2 && n == 0);
  assert (mulib_xtks_read (&s, output, 1, &n) == -2 && n == 0);
  off = 2;
  assert (mulib_xtks_write_all (&s, "x", 1, &off) == -1 && off == 2);
}
typedef struct packet_sink
{
  size_t count, bytes;
  int stop;
} packet_sink;
static int
packet (void *ctx, const void *p, size_t n)
{
  packet_sink *s = ctx;
  if (n)
    assert (n == 3 && !memcmp (p, "abc", 3));
  ++s->count;
  s->bytes += n;
  return s->stop;
}
static void
frames (void)
{
  unsigned char bytes[15], storage[3];
  mulib_packetstream_t s;
  packet_sink sink = { 0 };
  size_t n, used;
  assert (mulib_packetstream_encode ("abc", 3, bytes, sizeof bytes, &n)
          && n == 7);
  assert (mulib_packetstream_encode (NULL, 0, bytes + 7, 8, &n) && n == 4);
  assert (mulib_packetstream_encode (NULL, 0, bytes + 11, 4, &n));
  for (size_t split = 0; split <= 15; ++split)
    {
      sink.count = sink.bytes = 0;
      assert (mulib_packetstream_init (&s, storage, sizeof storage));
      assert (!mulib_packetstream_feed (&s, bytes, split, &used, packet, &sink)
              && used == split);
      assert (!mulib_packetstream_feed (&s, bytes + split, 15 - split, &used,
                                        packet, &sink));
      assert (sink.count == 3 && sink.bytes == 3
              && mulib_packetstream_finished (&s));
    }
  assert (mulib_packetstream_init (&s, storage, 3));
  sink.stop = 1;
  assert (mulib_packetstream_feed (&s, bytes, 15, &used, packet, &sink) == 1
          && used == 7);
  assert (mulib_packetstream_finished (&s));
  assert (mulib_packetstream_init (&s, storage, 2));
  assert (mulib_packetstream_feed (&s, bytes, 7, &used, packet, &sink) == -2
          && used == 4);
  assert (!mulib_packetstream_finished (&s));
  assert (mulib_packetstream_feed (&s, bytes, 7, &used, packet, &sink) == -2
          && used == 0);
  assert (mulib_packetstream_init (&s, storage, 3));
  assert (!mulib_packetstream_feed (&s, bytes, 6, &used, packet, &sink));
  assert (!mulib_packetstream_finished (&s));
}
static void
relay (void)
{
  memory_io a = { 0 }, b = { 0 };
  unsigned char ab[4], ba[4];
  mulib_tunnel_t t;
  int r = 0;
  mulib_xtks_t sa = { &a, read_memory, write_memory },
               sb = { &b, read_memory, write_memory };
  a.input = (const unsigned char *)"left to right";
  a.input_size = 13;
  b.input = (const unsigned char *)"right to left";
  b.input_size = 13;
  assert (!mulib_tunnel_init (&t, &sa, &sb, ab, ba, sizeof ab));
  for (unsigned i = 0; i < 100 && r != 2; ++i)
    {
      if (i % 3 == 0)
        b.block = 1;
      r = mulib_tunnel_step (&t);
      assert (r >= 0);
    }
  assert (r == 2 && a.write_pos == 13 && b.write_pos == 13);
  assert (!memcmp (a.output, b.input, 13) && !memcmp (b.output, a.input, 13));
  a.bad = 1;
  assert (!mulib_tunnel_init (&t, &sa, &sb, ab, ba, sizeof ab));
  assert (mulib_tunnel_step (&t) < 0 && mulib_tunnel_step (&t) < 0);
}
static int
handshake (void *ctx, const unsigned char certificate[52], int *verified)
{
  memory_io *m = ctx;
  assert (certificate[0] == 42);
  if (!m->handshakes++)
    return 1;
  *verified = m->auth;
  return 0;
}
static void
close_memory (void *ctx)
{
  ++((memory_io *)ctx)->closed;
}
static void
obfs (void)
{
  memory_io m = { 0 };
  mulib_obs4_t s = { 0 };
  unsigned char cert[52] = { 42 };
  size_t n;
  char bytes[16];
  mulib_obs4_ops_t ops
      = { handshake, read_memory, write_memory, close_memory };
  mulib_xtks_t io;
  assert (mulib_obs4_init (&s, &ops, &m, cert));
  cert[0] = 0;
  io = mulib_obs4_stream (&s);
  assert (mulib_xtks_read (&io, bytes, sizeof bytes, &n) < 0);
  assert (mulib_obs4_handshake (&s) == 1);
  assert (mulib_obs4_handshake (&s) == -2);
  m.auth = 1;
  assert (mulib_obs4_handshake (&s) == -2); /* failure sticky */
  mulib_obs4_close (&s);
  mulib_obs4_close (&s);
  assert (m.closed == 1);
  cert[0] = 42;
  m.handshakes = 0;
  assert (mulib_obs4_init (&s, &ops, &m, cert));
  assert (mulib_obs4_handshake (&s) == 1);
  assert (!mulib_obs4_handshake (&s));
  assert (!mulib_xtks_write (&io, "abc", 3, &n) && n == 2);
  m.bad = 1;
  assert (mulib_xtks_write (&io, "abc", 3, &n) < 0);
  assert (s.state == 3);
  mulib_obs4_close (&s);
  assert (m.closed == 2);
}
typedef struct lifecycle_fixture
{
  mulib_tsl_t tls;
  mulib_obs4_t obfs;
  int is_obfs, probe, cancel, closed, calls;
} lifecycle_fixture;
static int
lifecycle_handshake_call (lifecycle_fixture *f)
{
  return f->is_obfs ? mulib_obs4_handshake (&f->obfs)
                    : mulib_tsl_handshake (&f->tls);
}
static int
lifecycle_io_call (lifecycle_fixture *f, int write, size_t length, size_t *n)
{
  char byte = 'x';
  if (f->is_obfs)
    {
      mulib_xtks_t io = mulib_obs4_stream (&f->obfs);
      return write ? mulib_xtks_write (&io, &byte, length, n)
                   : mulib_xtks_read (&io, &byte, length, n);
    }
  return write ? mulib_tsl_write (&f->tls, &byte, length, n)
               : mulib_tsl_read (&f->tls, &byte, length, n);
}
static void
lifecycle_close_call (lifecycle_fixture *f)
{
  if (f->is_obfs)
    mulib_obs4_close (&f->obfs);
  else
    mulib_tsl_close (&f->tls);
}
static void
lifecycle_probe (lifecycle_fixture *f)
{
  size_t n = 99;
  ++f->calls;
  if (!f->probe)
    return;
  assert (lifecycle_handshake_call (f) == -1);
  assert (lifecycle_io_call (f, 0, 1, &n) == -1 && !n);
  assert (lifecycle_io_call (f, 1, 1, &n) == -1 && !n);
  if (f->cancel)
    {
      lifecycle_close_call (f);
      lifecycle_close_call (f);
      assert (!f->closed); /* backend is live until this callback returns */
      assert (lifecycle_handshake_call (f) == -1);
    }
}
static int
lifecycle_handshake (void *ctx, int *verified)
{
  lifecycle_probe (ctx);
  *verified = 1;
  return 0;
}
static int
lifecycle_obfs_handshake (void *ctx, const unsigned char cert[52], int *verified)
{
  assert (cert[0] == 42);
  return lifecycle_handshake (ctx, verified);
}
static int
lifecycle_read (void *ctx, void *p, size_t n, size_t *count)
{
  lifecycle_probe (ctx);
  assert (n == 1);
  *(char *)p = 'x';
  *count = 1;
  return 0;
}
static int
lifecycle_write (void *ctx, const void *p, size_t n, size_t *count)
{
  lifecycle_probe (ctx);
  assert (n == 1 && *(const char *)p == 'x');
  *count = 1;
  return 0;
}
static void
lifecycle_close (void *ctx)
{
  lifecycle_fixture *f = ctx;
  size_t n;
  assert (++f->closed == 1);
  lifecycle_close_call (f); /* close itself must be idempotent during teardown */
  assert (lifecycle_handshake_call (f) == -1);
  assert (lifecycle_io_call (f, 0, 1, &n) == -1 && !n);
}
static void
lifecycles (void)
{
  const mulib_tsl_ops_t tls_ops
      = { lifecycle_handshake, lifecycle_read, lifecycle_write, lifecycle_close };
  const mulib_obs4_ops_t obfs_ops
      = { lifecycle_obfs_handshake, lifecycle_read, lifecycle_write, lifecycle_close };
  unsigned char cert[52] = { 42 };
  for (int obfs = 0; obfs < 2; ++obfs)
    for (int phase = 0; phase < 3; ++phase)
      for (int cancel = 0; cancel < 2; ++cancel)
        {
          lifecycle_fixture f = { 0 };
          size_t n = 99;
          int result;
          f.is_obfs = obfs;
          assert (obfs ? mulib_obs4_init (&f.obfs, &obfs_ops, &f, cert)
                       : mulib_tsl_init (&f.tls, &tls_ops, &f));
          if (phase)
            {
              assert (!lifecycle_handshake_call (&f));
              assert (!lifecycle_io_call (&f, 0, 0, &n) && !n);
              assert (!lifecycle_io_call (&f, 1, 0, &n) && !n);
              assert (f.calls == 1); /* empty I/O never reaches the backend */
            }
          f.probe = 1;
          f.cancel = cancel;
          result = phase ? lifecycle_io_call (&f, phase == 2, 1, &n)
                         : lifecycle_handshake_call (&f);
          assert (result == (cancel ? -1 : 0));
          if (phase)
            assert (n == (cancel ? 0u : 1u));
          assert (f.closed == cancel);
          assert ((obfs ? f.obfs.state : f.tls.state) == (cancel ? 0 : 2));
          lifecycle_close_call (&f);
          lifecycle_close_call (&f);
          assert (f.closed == 1);
        }
}
/* Deterministic tag fixture for testing framing/nonce transitions ONLY.
   It is intentionally not an encryption algorithm. */
static int
seal_fixture (void *ctx, const unsigned char nonce[12], const void *p,
              size_t n, void *out)
{
  unsigned char *q = out;
  unsigned char tag = 0;
  const unsigned char *b = p;
  (void)ctx;
  for (unsigned i = 0; i < 12; ++i)
    tag ^= nonce[i];
  for (size_t i = 0; i < n; ++i)
    tag ^= b[i];
  if (n)
    memcpy (q, p, n);
  memset (q + n, tag, 16);
  return 1;
}
static int
open_fixture (void *ctx, const unsigned char nonce[12], const void *p,
              size_t n, void *out)
{
  const unsigned char *b = p;
  unsigned char tag = 0;
  (void)ctx;
  assert (n >= 16);
  for (unsigned i = 0; i < 12; ++i)
    tag ^= nonce[i];
  for (size_t i = 0; i < n - 16; ++i)
    tag ^= b[i];
  for (size_t i = n - 16; i < n; ++i)
    if (b[i] != tag)
      return 0;
  if (n > 16)
    memcpy (out, p, n - 16);
  return 1;
}
static void
shadowsocks (void)
{
  mulib_shadowsocks_t tx, rx;
  unsigned char wire[128], out[64];
  size_t n, used, got;
  assert (mulib_shadowsocks_init (&tx, 0, seal_fixture, NULL));
  assert (mulib_shadowsocks_init (&rx, 1, open_fixture, NULL));
  assert (mulib_shadowsocks_seal (&tx, "hello", 5, wire, sizeof wire, &n) == 1
          && n == 39 && tx.nonce == 2);
  assert (wire[0] == 0 && wire[1] == 5);
  for (size_t i = 0; i < n; ++i)
    {
      assert (
          mulib_shadowsocks_open (&rx, wire, i, out, sizeof out, &got, &used)
          == 0);
      assert (!rx.nonce && !got && !used);
    }
  assert (mulib_shadowsocks_open (&rx, wire, n, out, 4, &got, &used) == -1
          && !rx.nonce);
  assert (mulib_shadowsocks_open (&rx, wire, n, out, sizeof out, &got, &used)
          == 1);
  assert (rx.nonce == 2 && got == 5 && used == n && !memcmp (out, "hello", 5));
  assert (mulib_shadowsocks_seal (&tx, NULL, 0, wire, sizeof wire, &n) == 1
          && n == 34);
  assert (mulib_shadowsocks_open (&rx, wire, n, NULL, 0, &got, &used) == 1
          && got == 0 && rx.nonce == 4);
  assert (mulib_shadowsocks_seal (&tx, "secret", 6, wire, sizeof wire, &n)
          == 1);
  wire[n - 1] ^= 1;
  memset (out, 0x55, sizeof out);
  assert (mulib_shadowsocks_open (&rx, wire, n, out, sizeof out, &got, &used)
              == -2
          && rx.failed);
  for (size_t i = 0; i < sizeof out; ++i)
    assert (out[i] == 0x55);
  wire[n - 1] ^= 1;
  assert (mulib_shadowsocks_open (&rx, wire, n, out, sizeof out, &got, &used)
          == -2);
  tx.nonce = UINT64_MAX - 1;
  assert (mulib_shadowsocks_seal (&tx, "x", 1, wire, sizeof wire, &n) == -2
          && tx.failed);
}
static int
bad_packet_read (void *ctx, void *p, size_t n, size_t *got)
{
  (void)ctx;
  (void)p;
  *got = n + 1;
  return 0;
}
static int
packet_send (void *ctx, const void *p, size_t n)
{
  (void)ctx;
  (void)p;
  (void)n;
  return 0;
}
static void
packets (void)
{
  mulib_abstun_t t = { NULL, 4, bad_packet_read, packet_send };
  char p[8];
  size_t got;
  assert (mulib_abstun_receive (&t, p, 3, &got) == -1 && !got);
  assert (mulib_abstun_receive (&t, p, 8, &got) == -2 && !got);
  assert (mulib_abstun_send (&t, "12345", 5) == -1);
  assert (!mulib_abstun_send (&t, "1234", 4));
}
int
main (void)
{
  streams ();
  frames ();
  relay ();
  obfs ();
  lifecycles ();
  shadowsocks ();
  packets ();
  return 0;
}
