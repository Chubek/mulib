#include "mulib-sshtransport.h"
#include <assert.h>
#include <string.h>

#define SLICE(text) {(const unsigned char *)(text), sizeof (text) - 1}

static void
hex (void *out, size_t n, const char *str)
{
  unsigned char *p = out;
  const char alphabet[] = "0123456789abcdef";
  assert (strlen (str) == 2 * n);
  for (size_t i = 0; i < n; ++i) {
    const char *a = strchr (alphabet, str[2 * i]), *b = strchr (alphabet, str[2 * i + 1]);
    assert (a && b);
    p[i] = (unsigned char)(16 * (a - alphabet) + (b - alphabet));
  }
}

static void
equal_hex (const void *out, size_t n, const char *str)
{
  unsigned char expected[214];
  assert (n <= sizeof expected);
  hex (expected, n, str);
  assert (!memcmp (out, expected, n));
}

static void
ed25519 (unsigned char key[32])
{
  /* RFC 8032 section 7.1, TEST 1 (zero-length message). */
  unsigned char seed[32], sig[64];
  hex (seed, sizeof seed, "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
  assert (mulib_ed25519_public (key, seed));
  equal_hex (key, 32, "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
  assert (mulib_ed25519_sign (sig, seed, NULL, 0));
  equal_hex (sig, sizeof sig, "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
                              "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
  assert (mulib_ed25519_verify (sig, key, NULL, 0));
  sig[0] ^= 1;
  assert (!mulib_ed25519_verify (sig, key, NULL, 0));
}

static void
x25519 (unsigned char client[32], unsigned char server[32])
{
  /* RFC 7748 section 6.1, Alice and Bob's shared secret. */
  unsigned char alice[32], bob[32], shared[32], zero[32] = {0};
  hex (alice, 32, "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a");
  hex (bob, 32, "5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb");
  assert (mulib_x25519_public (client, alice));
  equal_hex (client, 32, "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a");
  assert (mulib_x25519_public (server, bob));
  equal_hex (server, 32, "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f");
  assert (mulib_x25519 (shared, alice, server));
  equal_hex (shared, 32, "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742");
  assert (mulib_x25519 (shared, bob, client));
  equal_hex (shared, 32, "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742");
  assert (!mulib_x25519 (shared, alice, zero));
}

static void
transcript (const unsigned char host[32], const unsigned char client[32],
            const unsigned char server[32])
{
  /* RFC 4253/8731 independent fixtures: Python struct.pack(">I", len(x))
   * and hashlib.sha256 over the exact strings and mpint. Synthetic K has
   * its high bit set in byte zero, requiring the big-endian mpint sign byte.
   * The server proposal encodes a true follows field as 0x02. */
  unsigned char ic[168], is[214], k[32], sid[32], h[32], changed[32], derived[73];
  const char vc[] = "SSH-2.0-vector_client build 7";
  const char vs[] = "SSH-2.0-vector_server internal tests";
  const mulib_ssh_slice_t vc_crlf = SLICE ("SSH-2.0-vector_client build 7\r\n");
  const mulib_ssh_slice_t vs_crlf = SLICE ("SSH-2.0-vector_server internal tests\r\n");
  const mulib_ssh_slice_t no_comment = SLICE ("SSH-2.0-vector_client");
  mulib_ssh_transcript_t t = { SLICE (vc), SLICE (vs), {ic, sizeof ic}, {is, sizeof is} };
  mulib_ssh_transcript_t variant = t;
  hex (ic, sizeof ic, "14000102030405060708090a0b0c0d0e0f00000011637572766532353531392d736861323536"
                      "0000000b7373682d65643235353139000000166165733132382d67636d406f70656e7373682e636f6d"
                      "000000166165733132382d67636d406f70656e7373682e636f6d0000000d686d61632d736861322d323536"
                      "0000000d686d61632d736861322d323536000000046e6f6e65000000046e6f6e6500000000000000000000000000");
  hex (is, sizeof is, "14101112131415161718191a1b1c1d1e1f0000002f6469666669652d68656c6c6d616e2d67726f757031342d"
                      "7368613235362c637572766532353531392d7368613235360000000b7373682d656432353531390000002161657331"
                      "32382d67636d406f70656e7373682e636f6d2c6165733235362d637472000000166165733132382d67636d406f7065"
                      "6e7373682e636f6d0000000d686d61632d736861322d3235360000000d686d61632d736861322d323536000000046e"
                      "6f6e65000000046e6f6e6500000005656e2d5553000000000200000000");
  hex (k, sizeof k, "ca5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742");
  for (size_t i = 0; i < sizeof sid; ++i) sid[i] = (unsigned char)i;
  assert (mulib_sshkex_hash (&t, host, client, server, k, h));
  equal_hex (h, sizeof h, "4a5e895d19e2fcd634a87685a241abf1ed1c3f8f7546c55271fd16316074cf47");
  variant.client_version = vc_crlf;
  variant.server_version = vs_crlf;
  assert (mulib_sshkex_hash (&variant, host, client, server, k, changed));
  equal_hex (changed, sizeof changed, "dc7b464194f6962da9687f3c663e02c9e47e5982d0fe2eba555067f089562ec4");
  variant = t;
  variant.client_version = no_comment;
  assert (mulib_sshkex_hash (&variant, host, client, server, k, changed));
  equal_hex (changed, sizeof changed, "7065a1ca19cbeef14218c8eee368ad5f4bdfe581834e9d0b98028c221e71aee2");
  int skip;
  assert (mulib_sshkex_negotiate (ic, sizeof ic, &skip) && !skip);
  assert (mulib_sshkex_negotiate (is, sizeof is, &skip) && skip);
  is[sizeof is - 5] = 1;
  assert (mulib_sshkex_negotiate (is, sizeof is, &skip) && skip);
  assert (mulib_sshkex_hash (&t, host, client, server, k, changed));
  equal_hex (changed, sizeof changed, "708627d84682fa6c6c322cfd6ed420db93fe858ffde378c22b8ff3a8ad275f46");
  const mulib_ssh_slice_t a = t.client_version, b = t.server_version;
  mulib_sshtransport_t s;
  unsigned char frame[MULIB_SSH_KEX_OUTPUT_MIN];
  size_t n;
  assert (!mulib_sshtransport_start (&s, MULIB_SSH_CLIENT, vc_crlf, b, host, frame, sizeof frame, &n));
  assert (!mulib_sshtransport_start (&s, MULIB_SSH_CLIENT, a, vs_crlf, host, frame, sizeof frame, &n));
  assert (mulib_sshtransport_start (&s, MULIB_SSH_CLIENT, a, b, host, frame, sizeof frame, &n));
  mulib_sshtransport_clear (&s);
  /* K1, K2, and a partial K3 under a session ID distinct from H. */
  assert (mulib_ssh_derive (k, h, sid, 'A', derived, sizeof derived));
  equal_hex (derived, sizeof derived, "f8be973cfdef763ee08a73e385123c949e4e0ada78cd7722e63d52ee235c323dd6"
                                       "ac2d0f0f53fe827b9bede1bbe2a42b073ba79f702537ccc1600ceea65d144fa5c"
                                       "9f315f592a42623");
  const char *const selectors[] = {
    "c28799cfcb3e912d5b4bb54420be12c31f798251c8e21644f259c71d673c73e4",
    "84f79f8b47d743e0ce9ed9391579f58d93ae617b576280241ce2f87928403a12",
    "2331e0b1e77330ab4fda5d2d5aeeccda429dd46f2ec63d176b53fdeca0d52a2c",
    "50dee2ec815f1130af362cb274bde1a36a466c6d366b858e401927ed72f34f74",
    "e650b02c5f04af9deb5e32428bb0d519c824974f9072474d54153e35bd3f2af1"
  };
  for (size_t i = 0; i < sizeof selectors / sizeof selectors[0]; ++i) {
    assert (mulib_ssh_derive (k, h, sid, (unsigned char)('B' + i), derived, 32));
    equal_hex (derived, 32, selectors[i]);
  }
}

int
main (void)
{
  unsigned char host[32], client[32], server[32];
  ed25519 (host);
  x25519 (client, server);
  transcript (host, client, server);
  return 0;
}
