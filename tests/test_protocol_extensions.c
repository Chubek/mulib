#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_WEBCLIENT
#define MULIB_IMPL_TSL
#define MULIB_IMPL_HTTPS
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_GIT
#define MULIB_IMPL_VARINT
#define MULIB_IMPL_SOCKS5
#define MULIB_IMPL_V2RAYPROTO
#define MULIB_IMPL_SSHCLIENT
#define MULIB_IMPL_SSHSERVER
#define MULIB_IMPL_PACKETFITER
#define MULIB_IMPL_PACKETRULES
#define MULIB_IMPL_PACKETVM
#define MULIB_IMPL_OPENVPN
#define MULIB_IMPL_BITTORRENT
#define MULIB_IMPL_IPFS
#define MULIB_IMPL_P2PPROTO
#include "mulib-bittorrent.h"
#include "mulib-https.h"
#include "mulib-ipfs.h"
#include "mulib-openvpn.h"
#include "mulib-p2pproto.h"
#include "mulib-packetrules.h"
#include "mulib-packetvm.h"
#include "mulib-sshclient.h"
#include "mulib-sshserver.h"
#include "mulib-v2rayproto.h"
#include <assert.h>
#include <string.h>

static void
socks (void)
{
  unsigned char wire[300], reply[2];
  size_t n, used;
  unsigned command;
  mulib_socks5_address_t a = { 0 }, b;
  const unsigned char greeting[] = { 5, 3, 0, 1, 2 }, preferred[] = { 2, 0 };
  for (size_t i = 0; i < sizeof greeting; ++i)
    assert (mulib_socks5_negotiate (greeting, i, preferred, 2, reply, &used)
                == 0
            && used == 0);
  assert (mulib_socks5_negotiate (greeting, sizeof greeting, preferred, 2,
                                  reply, &used)
              == 1
          && reply[1] == 2);
  assert (
      mulib_socks5_negotiate (greeting, sizeof greeting, NULL, 0, reply, &used)
          == 1
      && reply[1] == 255);
  for (unsigned type = 1; type <= 4; ++type)
    {
      if (type == 2)
        continue;
      a.type = type;
      a.port = 65535;
      a.length = type == 1 ? 4 : type == 3 ? 255 : 16;
      memset (a.host, 'x', sizeof a.host);
      for (unsigned is_reply = 0; is_reply < 2; ++is_reply)
        {
          assert (mulib_socks5_encode (1, (int)is_reply, &a, wire, sizeof wire,
                                       &n));
          for (size_t i = 0; i < n; ++i)
            assert (mulib_socks5_decode (wire, i, (int)is_reply, &command, &b,
                                         &used)
                        == 0
                    && used == 0);
          assert (
              mulib_socks5_decode (wire, n, (int)is_reply, &command, &b, &used)
                  == 1
              && used == n);
          assert (b.port == a.port && b.length == a.length
                  && !memcmp (b.host, a.host, a.length));
        }
    }
  wire[2] = 1;
  assert (mulib_socks5_decode (wire, n, 1, &command, &b, &used) == -1);
  a.type = 3;
  a.length = 2;
  a.host[0] = 'a';
  a.host[1] = 0;
  assert (!mulib_socks5_address_encode (&a, wire, sizeof wire, &used));
  a.host[1] = 'b';
  {
    mulib_v2rayproto_request_t v = { { 0 }, 1, { 0 } }, decoded;
    v.address = a;
    v.uuid[15] = 42;
    assert (mulib_v2rayproto_encode (&v, wire, sizeof wire, &n));
    for (size_t i = 0; i < n; ++i)
      assert (mulib_v2rayproto_decode (wire, i, &decoded, &used) == 0);
    assert (mulib_v2rayproto_decode (wire, n, &decoded, &used) == 1
            && used == n && decoded.uuid[15] == 42);
    wire[17] = 1;
    assert (mulib_v2rayproto_decode (wire, n, &decoded, &used) == -2);
  }
}
static int
padding (void *ctx, void *p, size_t n)
{
  (void)ctx;
  memset (p, 0xab, n);
  return 1;
}
static int
verify (void *ctx, const mulib_sshserver_auth_t *auth)
{
  ++*(int *)ctx;
  return auth->password_length == 1 && auth->password[0] == 'p';
}
static void
ssh (void)
{
  unsigned char wire[128];
  char software[64];
  const unsigned char *payload;
  size_t n, used, length;
  mulib_sshserver_t s;
  int calls = 0;
  const unsigned char auth[] = { 50,  0,   0,   0,   1,   'u', 0,   0,   0,
                                 14,  's', 's', 'h', '-', 'c', 'o', 'n', 'n',
                                 'e', 'c', 't', 'i', 'o', 'n', 0,   0,   0,
                                 8,   'p', 'a', 's', 's', 'w', 'o', 'r', 'd',
                                 0,   0,   0,   0,   1,   'p' };
  assert (mulib_sshclient_identification ("client", wire, sizeof wire, &n));
  for (size_t i = 0; i < n; ++i)
    assert (
        mulib_sshclient_identify (wire, i, software, sizeof software, &used)
        == 0);
  assert (mulib_sshclient_identify (wire, n, software, sizeof software, &used)
              == 1
          && !strcmp (software, "client"));
  assert (
      !mulib_sshclient_identification ("bad\nname", wire, sizeof wire, &used));
  assert (mulib_sshclient_identify ("SSH-1.5-client\r\n", 16, software,
                                    sizeof software, &used)
          == -1);
  assert (mulib_sshclient_packet_encode (auth, sizeof auth, wire, sizeof wire,
                                         &n, padding, NULL));
  for (size_t i = 0; i < n; ++i)
    assert (mulib_sshclient_packet_decode (wire, i, &payload, &length, &used)
            == 0);
  assert (mulib_sshclient_packet_decode (wire, n, &payload, &length, &used)
              == 1
          && length == sizeof auth);
  assert (mulib_sshserver_init (&s, 2));
  assert (
      mulib_sshserver_authenticate (&s, auth, sizeof auth - 1, verify, &calls)
          == 0
      && calls == 0);
  assert (mulib_sshserver_authenticate (&s, payload, length, verify, &calls)
              == 1
          && calls == 1);
  assert (mulib_sshserver_authenticate (&s, payload, length, verify, &calls)
              == -1
          && calls == 1);
  wire[4] = 3;
  assert (mulib_sshclient_packet_decode (wire, n, &payload, &length, &used)
          == -1);
  assert (mulib_sshserver_init (&s, 1));
  assert (mulib_sshserver_authenticate (&s, NULL, 0, verify, &calls) == 0);
  assert (mulib_sshserver_authenticate (&s, auth, sizeof auth, verify, &calls)
          == -1);
}
static void
filters (void)
{
  unsigned char udp[28]
      = { 0x45, 0, 0,   28, 0,   0, 0,    0,    64, 17, 0, 0, 192, 0,
          2,    7, 198, 51, 100, 1, 0x12, 0x34, 0,  53, 0, 8, 0,   0 };
  mulib_packetfiter_view_t v;
  mulib_packetrules_rule_t rule = { 0 };
  size_t matched;
  mulib_packetvm_instruction_t code[]
      = { { 1, 9, 0, 0 }, { 5, 17, 1, 0 }, { 6, 0, 0, 0 }, { 6, 1, 0, 0 } };
  int verdict = 99;
  for (size_t i = 0; i < sizeof udp; ++i)
    assert (mulib_packetfiter_parse (udp, i, &v) == 0);
  assert (mulib_packetfiter_parse (udp, sizeof udp, &v) == 1 && v.has_ports
          && v.payload_offset == 28);
  rule.family = 4;
  rule.protocol = 17;
  rule.source[0] = 192;
  rule.source[1] = 0;
  rule.source[2] = 2;
  rule.source_prefix = 24;
  rule.ports = 1;
  rule.source_high = 65535;
  rule.destination_low = rule.destination_high = 53;
  rule.action = 1;
  assert (mulib_packetrules_apply (&rule, 1, 0, udp, sizeof udp, &matched) == 1
          && matched == 0);
  udp[6] = 0x20;
  assert (mulib_packetfiter_parse (udp, sizeof udp, &v) == 1 && v.fragmented
          && !v.has_ports);
  assert (mulib_packetrules_apply (&rule, 1, 0, udp, sizeof udp, &matched) == 0
          && matched == SIZE_MAX);
  {
    mulib_packetfiter_view_t before = v;
    udp[6] = 0x60; /* DF contradicts MF. */
    assert (!mulib_packetfiter_parse (udp, sizeof udp, &v));
    udp[6] = 0x20;
    udp[3] = 27; /* Nonfinal fragment payload must be a multiple of eight. */
    assert (!mulib_packetfiter_parse (udp, sizeof udp, &v));
    udp[3] = 20; /* Empty fragments cannot contribute to reassembly. */
    assert (!mulib_packetfiter_parse (udp, sizeof udp, &v));
    udp[3] = 28;
    udp[6] = 0x1f;
    udp[7] = 0xff; /* Offset plus payload exceeds the IP datagram limit. */
    assert (!mulib_packetfiter_parse (udp, sizeof udp, &v));
    assert (!memcmp (&v, &before, sizeof v));
    udp[6] = 0;
    udp[7] = 1; /* A final fragment may have a non-multiple-of-eight size. */
    udp[3] = 27;
    assert (mulib_packetfiter_parse (udp, sizeof udp, &v) == 1
            && v.fragmented && !v.has_ports);
    udp[7] = 0;
    udp[3] = 28;
  }
  udp[6] = 0;
  udp[27] = 1;
  udp[25] = 9;
  assert (mulib_packetfiter_parse (udp, sizeof udp, &v) == 0);
  udp[25] = 8;
  rule.source_prefix = 33;
  assert (!mulib_packetrules_validate (&rule, 1));
  assert (mulib_packetvm_run (code, 4, udp, sizeof udp, 4, &verdict) == 0
          && verdict == 1);
  verdict = 99;
  assert (mulib_packetvm_run (code, 4, udp, 9, 4, &verdict) == -3
          && verdict == 99);
  assert (mulib_packetvm_run (code, 4, udp, sizeof udp, 1, &verdict) == -2
          && verdict == 99);
  code[1].yes = SIZE_MAX;
  assert (!mulib_packetvm_validate (code, 4, &matched) && matched == 1);
  {
    unsigned char ipv6[48] = { 0x60, 0, 0, 0, 0, 8, 17, 64 };
    ipv6[45] = 8;
    assert (mulib_packetfiter_parse (ipv6, sizeof ipv6, &v) == 1
            && v.family == 6);
    const unsigned extensions[]
        = { 0, 43, 44, 50, 51, 60, 135, 139, 140, 253, 254 };
    for (size_t i = 0; i < sizeof extensions / sizeof *extensions; ++i)
      {
        ipv6[6] = (unsigned char)extensions[i];
        assert (mulib_packetfiter_parse (ipv6, sizeof ipv6, &v) == -2);
      }
  }
}
static void
peer_protocols (void)
{
  mulib_openvpn_replay_t replay = { 0 };
  unsigned char wire[256];
  size_t n, used;
  char token[128];
  mulib_bittorrent_message_t m = { 6, NULL, 12 }, decoded;
  unsigned char request[12] = { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0x40, 0 };
  m.data = request;
  assert (mulib_bittorrent_encode (&m, wire, sizeof wire, &n));
  for (size_t i = 0; i < n; ++i)
    assert (mulib_bittorrent_decode (wire, i, &decoded, &used) == 0
            && used == 0);
  assert (mulib_bittorrent_decode (wire, n, &decoded, &used) == 1
          && decoded.id == 6);
  request[11] = 1;
  assert (!mulib_bittorrent_encode (&m, wire, sizeof wire, &used));
  request[10] = request[11] = 0;
  assert (!mulib_bittorrent_encode (&m, wire, sizeof wire, &used));
  assert (mulib_openvpn_replay_accept (&replay, 100));
  assert (mulib_openvpn_replay_accept (&replay, 37));
  assert (!mulib_openvpn_replay_accept (&replay, 36));
  assert (!mulib_openvpn_replay_accept (&replay, 37));
  assert (mulib_openvpn_replay_accept (&replay, 200));
  assert (!mulib_openvpn_replay_accept (&replay, 100));
  assert (mulib_openvpn_replay_accept (&replay, UINT32_MAX));
  assert (!mulib_openvpn_replay_accept (&replay, 1));
  assert (mulib_p2pproto_encode ("/multistream/1.0.0", wire, sizeof wire, &n));
  for (size_t i = 0; i < n; ++i)
    assert (mulib_p2pproto_decode (wire, i, token, sizeof token, &used) == 0);
  assert (mulib_p2pproto_decode (wire, n, token, sizeof token, &used) == 1
          && used == n);
  assert (!strcmp (token, "/multistream/1.0.0"));
  wire[n - 1] = 0;
  assert (mulib_p2pproto_decode (wire, n, token, sizeof token, &used) == -1);
  {
    const unsigned char bad[] = { 0x83, 0, 'n', 'a', '\n' };
    assert (mulib_p2pproto_decode (bad, sizeof bad, token, sizeof token, &used)
            == -1);
  }
  {
    mulib_ipfs_cid_t cid = { 0x70, { 0 } }, other;
    char text[60];
    assert (mulib_ipfs_cid_format (&cid, text));
    assert (mulib_ipfs_cid_parse (text, 59, &other));
    assert (other.codec == 0x70);
    text[58] = 'b';
    assert (!mulib_ipfs_cid_parse (text, 59, &other));
    assert (mulib_ipfs_cid_encode (&cid, wire));
    wire[3] = 31;
    assert (!mulib_ipfs_cid_decode (wire, 36, &other));
  }
}
typedef struct tls_fixture
{
  int verify, closed, writes, mode;
  size_t position;
} tls_fixture;
static int
tls_handshake (void *ctx, int *verified)
{
  *verified = ((tls_fixture *)ctx)->verify;
  return 0;
}
static int
tls_read (void *ctx, void *out, size_t cap, size_t *count)
{
  tls_fixture *f = ctx;
  const char *reply = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
  size_t n = strlen (reply);
  *count = 0;
  if (f->position == n)
    return 2;
  if (f->mode == 2)
    return 1;
  if (cap > 3)
    cap = 3;
  if (cap > n - f->position)
    cap = n - f->position;
  memcpy (out, reply + f->position, cap);
  f->position += cap;
  *count = cap;
  return 0;
}
static int
tls_write (void *ctx, const void *p, size_t n, size_t *count)
{
  tls_fixture *f = ctx;
  (void)p;
  ++f->writes;
  *count = f->mode == 1 ? 0 : n > 7 ? 7 : n;
  return 0;
}
static void
tls_close (void *ctx)
{
  ++((tls_fixture *)ctx)->closed;
}
static int
tls_connect (void *ctx, const mulib_uri_t *endpoint, mulib_tsl_t *s)
{
  mulib_tsl_ops_t ops = { tls_handshake, tls_read, tls_write, tls_close };
  assert (endpoint->scheme.length == 5 && endpoint->host.length == 12);
  assert (!memcmp (endpoint->host.data, "example.test", 12));
  assert (endpoint->has_port && endpoint->port.length == 3
          && !memcmp (endpoint->port.data, "443", 3));
  return mulib_tsl_init (s, &ops, ctx);
}
static void
https (void)
{
  mulib_http_message_t response = { 0 };
  tls_fixture f = { 0 };
  assert (mulib_https_request ("https://example.test/", "GET", NULL, 0, NULL,
                               0, NULL, tls_connect, &f, &response)
          == MULIB_WEBCLIENT_TRANSPORT);
  assert (f.closed == 1 && f.writes == 0 && !response.body);
  f.verify = 1;
  assert (mulib_https_request ("https://example.test/", "GET", NULL, 0, NULL,
                               0, NULL, tls_connect, &f, &response)
          == MULIB_WEBCLIENT_OK);
  assert (f.closed == 2 && response.body_length == 2
          && !memcmp (response.body, "ok", 2));
  mulib_http_message_free (&response);
  assert (mulib_https_request ("http://example.test/", "GET", NULL, 0, NULL, 0,
                               NULL, tls_connect, &f, &response)
          == MULIB_WEBCLIENT_INVALID);
  assert (mulib_https_request ("https://user@example.test/", "GET", NULL, 0,
                               NULL, 0, NULL, tls_connect, &f, &response)
          == MULIB_WEBCLIENT_INVALID);
  f.mode = 1;
  assert (mulib_https_request ("https://example.test/", "GET", NULL, 0, NULL,
                               0, NULL, tls_connect, &f, &response)
          == MULIB_WEBCLIENT_TRANSPORT);
  assert (f.closed == 3);
  f.mode = 2;
  f.position = 0;
  assert (mulib_https_request ("https://example.test/", "GET", NULL, 0, NULL,
                               0, NULL, tls_connect, &f, &response)
          == MULIB_WEBCLIENT_TRANSPORT);
  assert (f.closed == 4);
}
int
main (void)
{
  socks ();
  ssh ();
  filters ();
  peer_protocols ();
  https ();
  return 0;
}
